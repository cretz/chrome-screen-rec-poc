package main

import (
	"archive/tar"
	"archive/zip"
	"bufio"
	"bytes"
	"compress/gzip"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
)

func main() {
	if err := start(); err != nil {
		fmt.Printf("Error: %v\n", err)
		os.Exit(2)
	}
}

var debugEnabled = false

func start() error {
	if runtime.GOOS != "windows" && runtime.GOOS != "linux" {
		return fmt.Errorf("Unsupported OS '%v'", runtime.GOOS)
	}
	if runtime.GOARCH != "amd64" {
		return fmt.Errorf("Unsupported OS '%v' or arch '%v'", runtime.GOOS, runtime.GOARCH)
	}
	if len(os.Args) < 2 {
		return fmt.Errorf("No command provided")
	}

	switch os.Args[1] {
	case "rerun":
		err := clean()
		if err == nil {
			err = run()
		}
		return err
	case "run":
		return run()
	case "clean":
		return clean()
	case "rebuild":
		err := clean()
		if err == nil {
			err = build()
		}
		return err
	case "build":
		return build()
	case "package":
		return pkg()
	case "lint":
		return lint()
	case "unit-test":
		return unitTest()
	case "benchmark":
		return benchmark()
	default:
		return fmt.Errorf("Unrecognized command '%v'", os.Args[1])
	}
}

func run(extraQmakeArgs ...string) error {
	if err := build(extraQmakeArgs...); err != nil {
		return err
	}
	target, err := target()
	if err != nil {
		return err
	}
	return execCmd(filepath.Join(target, exeExt("attempt2")), extraArgs()...)
}

func clean() error {
	err := os.RemoveAll("debug")
	if err == nil {
		err = os.RemoveAll("release")
	}
	return err
}

func build(extraQmakeArgs ...string) error {
	target, err := target()
	if err != nil {
		return err
	}
	// Get qmake path
	qmakePath, err := exec.LookPath(exeExt("qmake"))
	if err != nil {
		return err
	}

	// Make the dir for the target
	if err := os.MkdirAll(target, 0755); err != nil {
		return err
	}

	// Run qmake TODO: put behind flag
	qmakeArgs := extraQmakeArgs
	if target == "debug" {
		qmakeArgs = append(qmakeArgs, "CONFIG+=debug")
	} else {
		qmakeArgs = append(qmakeArgs, "CONFIG+=release", "CONFIG-=debug")
	}
	qmakeArgs = append(qmakeArgs, "attempt2.pro")
	if err := execCmd(qmakePath, qmakeArgs...); err != nil {
		return fmt.Errorf("QMake failed: %v", err)
	}

	// Run nmake if windows, make if linux
	makeExe := "make"
	makeArgs := []string{}
	if runtime.GOOS == "windows" {
		makeExe = "nmake.exe"
		// This version takes the target name unlike the Linux one
		makeArgs = []string{target, "/NOLOGO"}
	}
	if err := execCmd(makeExe, makeArgs...); err != nil {
		return fmt.Errorf("NMake failed: %v", err)
	}

	// Chmod on linux
	if runtime.GOOS == "linux" {
		if err = os.Chmod(filepath.Join(target, "attempt2"), 0755); err != nil {
			return err
		}
	}

	// Copy over resources
	if err := copyResources(qmakePath, target); err != nil {
		return err
	}

	return nil
}

func pkg() error {
	target, err := target()
	if err != nil {
		return err
	}
	// Just move over the files that matter to a new deploy dir and zip em up
	deployDir := filepath.Join(target, "package", "attempt2")
	if err = os.MkdirAll(deployDir, 0755); err != nil {
		return err
	}

	// Get all base-dir items to copy, excluding only some
	filesToCopy := []string{}
	dirFiles, err := ioutil.ReadDir(target)
	if err != nil {
		return err
	}
	for _, file := range dirFiles {
		if !file.IsDir() {
			switch filepath.Ext(file.Name()) {
			case ".cpp", ".h", ".obj", ".res", ".manifest", ".log", ".o":
				// No-op
			default:
				filesToCopy = append(filesToCopy, file.Name())
			}
		}
	}
	if err = copyEachToDirIfNotPresent(target, deployDir, filesToCopy...); err != nil {
		return err
	}

	// And other dirs if present in folder
	subDirs := []string{"imageformats", "locales", "platforms", "sqldrivers"}
	for _, subDir := range subDirs {
		srcDir := filepath.Join(target, subDir)
		if _, err = os.Stat(srcDir); err == nil {
			if err = copyDirIfNotPresent(srcDir, filepath.Join(deployDir, subDir)); err != nil {
				return fmt.Errorf("Unable to copy %v: %v", subDir, err)
			}
		}
	}

	// Now create a zip or tar file with all the goods
	if runtime.GOOS == "windows" {
		err = createSingleDirZipFile(deployDir, filepath.Join(target, "package", "attempt2.zip"))
	} else {
		err = createSingleDirTarGzFile(deployDir, filepath.Join(target, "package", "attempt2.tar.gz"))
	}
	if err != nil {
		return err
	}

	return os.RemoveAll(deployDir)
}

func lint() error {
	toIgnore := []string{
		"No copyright message found.",
		"#ifndef header guard has wrong style, please use: ATTEMPT2_",
		"#endif line should be \"#endif  // ATTEMPT2_",
		"Include the directory when naming .h files",
		"Done processing",
		"Total errors found",
	}
	// Run lint on all cc and h files, and trim off any of the toIgnore stuff
	depotToolsDir := os.Getenv("DEPOT_TOOLS_DIR")
	if depotToolsDir == "" {
		return fmt.Errorf("Unable to find DEPOT_TOOLS_DIR env var")
	}
	args := []string{
		filepath.Join(depotToolsDir, "cpplint.py"),
		// Can't use, ref: https://github.com/google/styleguide/issues/22
		// "--root=attempt2\\",
	}
	integrationTestDir := filepath.Join("tests", "integration")
	err := filepath.Walk(".", func(path string, info os.FileInfo, err error) error {
		if !info.IsDir() && !strings.HasPrefix(info.Name(), "moc_") &&
			!strings.HasPrefix(path, integrationTestDir) &&
			(strings.HasSuffix(path, ".cc") || strings.HasSuffix(path, ".h")) {
			args = append(args, path)
		}
		return nil
	})
	if err != nil {
		return err
	}
	pycmd := "python"
	if runtime.GOOS == "linux" {
		// python by itself may refer to python3 or python2 depending on the distro,
		// so invoke python2 explicitly.
		pycmd = "python2"
	}
	cmd := exec.Command(pycmd, args...)
	out, err := cmd.CombinedOutput()
	if err != nil && len(out) == 0 {
		return fmt.Errorf("Unable to run cpplint: %v", err)
	}
	scanner := bufio.NewScanner(bytes.NewReader(out))
	foundAny := false
	for scanner.Scan() {
		// If after the trimmed string after the second colon starts w/ any toIgnore, we ignore it
		ignore := false
		origLine := scanner.Text()
		checkLine := origLine
		if firstColon := strings.Index(origLine, ":"); firstColon != -1 {
			if secondColon := strings.Index(origLine[firstColon+1:], ":"); secondColon != -1 {
				checkLine = strings.TrimSpace(origLine[firstColon+secondColon+2:])
			}
		}
		for _, toCheck := range toIgnore {
			if strings.HasPrefix(checkLine, toCheck) {
				ignore = true
				break
			}
		}
		if !ignore {
			fmt.Println(origLine)
			foundAny = true
		}
	}
	if foundAny {
		return fmt.Errorf("Lint check returned one or more errors")
	}
	return nil
}

func unitTest() error {
	if err := build("CONFIG+=test"); err != nil {
		return err
	}
	target, err := target()
	if err != nil {
		return err
	}
	return execCmd(filepath.Join(target, exeExt("attempt2-test")))
}

func benchmark() error {
	if err := build("CONFIG+=benchmark"); err != nil {
		return err
	}
	target, err := target()
	if err != nil {
		return err
	}
	return execCmd(filepath.Join(target, exeExt("attempt2-benchmark")))
}

func target() (string, error) {
	target := "debug"
	if len(os.Args) >= 3 && !strings.HasPrefix(os.Args[2], "--") {
		if os.Args[2] != "release" && os.Args[2] != "debug" {
			return "", fmt.Errorf("Unknown target '%v'", os.Args[2])
		}
		target = os.Args[2]
	}
	return target, nil
}

func extraArgs() []string {
	argStartIndex := 1
	if len(os.Args) >= 2 {
		argStartIndex = 2
		if len(os.Args) > 2 && (os.Args[2] == "release" || os.Args[2] == "debug") {
			argStartIndex = 3
		}
	}
	return os.Args[argStartIndex:]
}

func exeExt(baseName string) string {
	if runtime.GOOS == "windows" {
		return baseName + ".exe"
	}
	return baseName
}

func execCmd(name string, args ...string) error {
	return execCmdInDir("", name, args...)
}

func execCmdInDir(dir string, name string, args ...string) error {
	cmd := exec.Command(name, args...)
	cmd.Dir = dir
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	return cmd.Run()
}

func copyResources(qmakePath string, target string) error {
	if runtime.GOOS == "windows" {
		return copyResourcesWindows(qmakePath, target)
	}
	return copyResourcesLinux(qmakePath, target)
}

func copyResourcesLinux(qmakePath string, target string) error {
	if _, err := exec.LookPath("chrpath"); err != nil {
		return fmt.Errorf("Unable to find chrpath on the PATH: %v", err)
	}
	// Everything read only except by owner
	// Copy over some Qt DLLs
	err := copyAndChmodEachToDirIfNotPresent(0644, filepath.Join(filepath.Dir(qmakePath), "../lib"), target,
		"libQt5Core.so.5",
		"libQt5Gui.so.5",
		"libQt5Widgets.so.5",
		// TODO: See https://bugreports.qt.io/browse/QTBUG-53865
		"libicui18n.so.56",
		"libicuuc.so.56",
		"libicudata.so.56",
		// Needed for libqxcb platform
		"libQt5XcbQpa.so.5",
		"libQt5DBus.so.5",
	)
	if err != nil {
		return err
	}

	// Need some plugins
	// Before that, record whether the xcb plugin is there yet
	hadXcbPlugin := true
	xcbPluginPath := filepath.Join(target, "platforms", "libqxcb.so")
	if _, err = os.Stat(xcbPluginPath); os.IsNotExist(err) {
		hadXcbPlugin = false
	}
	if err = copyPlugins(qmakePath, target, "platforms", "qxcb"); err != nil {
		return fmt.Errorf("Unable to copy plugins: %v", err)
	}
	// If the xcb plugin wasn't there (but is now), change the rpath
	if !hadXcbPlugin {
		if err = execCmd("chrpath", "-r", "$ORIGIN/..", xcbPluginPath); err != nil {
			return fmt.Errorf("Unable to run chrpath: %v", err)
		}
	}
	return nil
}

func copyResourcesWindows(qmakePath string, target string) error {
	// Copy over some Qt DLLs
	qtDlls := []string{
		"Qt5Core.dll",
		"Qt5Gui.dll",
		"Qt5Widgets.dll",
		"Qt5WinExtras.dll",
	}
	// Debug libs are d.dll
	if target == "debug" {
		for i := range qtDlls {
			qtDlls[i] = strings.Replace(qtDlls[i], ".dll", "d.dll", -1)
		}
	}
	err := copyEachToDirIfNotPresent(filepath.Dir(qmakePath), target, qtDlls...)
	if err != nil {
		return err
	}

	// Need special ucrtbased.dll for debug builds
	if target == "debug" {
		err = copyEachToDirIfNotPresent("C:\\Program Files (x86)\\Windows Kits\\10\\bin\\x64\\ucrt",
			target, "ucrtbased.dll")
		if err != nil {
			return err
		}
	}

	// Need some plugins
	if err = copyPlugins(qmakePath, target, "platforms", "qwindows"); err != nil {
		return fmt.Errorf("Unable to copy plugins: %v", err)
	}
	return nil
}

func chmodEachInDir(mode os.FileMode, dir string, filenames ...string) error {
	for _, filename := range filenames {
		if err := os.Chmod(filepath.Join(dir, filename), mode); err != nil {
			return err
		}
	}
	return nil
}

func copyPlugins(qmakePath string, target string, dir string, plugins ...string) error {
	srcDir := filepath.Join(qmakePath, "../../plugins", dir)
	if _, err := os.Stat(srcDir); os.IsExist(err) {
		return fmt.Errorf("Unable to find Qt plugins dir %v: %v", dir, err)
	}
	destDir := filepath.Join(target, dir)
	if err := os.MkdirAll(destDir, 0755); err != nil {
		return fmt.Errorf("Unable to create dir: %v", err)
	}
	for _, plugin := range plugins {
		var fileName string
		if runtime.GOOS == "linux" {
			fileName = "lib" + plugin + ".so"
		} else if target == "debug" {
			fileName = plugin + "d.dll"
		} else {
			fileName = plugin + ".dll"
		}
		if err := copyAndChmodEachToDirIfNotPresent(0644, srcDir, destDir, fileName); err != nil {
			return err
		}
	}
	return nil
}

func copyDirIfNotPresent(srcDir string, destDir string) error {
	// Note, this is not recursive, but it does preserve permissions
	srcFi, err := os.Stat(srcDir)
	if err != nil {
		return fmt.Errorf("Unable to find src dir: %v", err)
	}
	if err = os.MkdirAll(destDir, srcFi.Mode()); err != nil {
		return fmt.Errorf("Unable to create dest dir: %v", err)
	}
	files, err := ioutil.ReadDir(srcDir)
	if err != nil {
		return fmt.Errorf("Unable to read src dir: %v", err)
	}
	for _, file := range files {
		srcFile := filepath.Join(srcDir, file.Name())
		if err = copyToDirIfNotPresent(srcFile, destDir); err != nil {
			return fmt.Errorf("Error copying file: %v", err)
		}
		if err = os.Chmod(srcFile, file.Mode()); err != nil {
			return fmt.Errorf("Unable to chmod file: %v", err)
		}
	}
	return nil
}

func copyAndChmodEachToDirIfNotPresent(mode os.FileMode, srcDir string, destDir string, srcFilenames ...string) error {
	if err := copyEachToDirIfNotPresent(srcDir, destDir, srcFilenames...); err != nil {
		return err
	}
	return chmodEachInDir(mode, destDir, srcFilenames...)
}

func copyEachToDirIfNotPresent(srcDir string, destDir string, srcFilenames ...string) error {
	for _, srcFilename := range srcFilenames {
		if err := copyToDirIfNotPresent(filepath.Join(srcDir, srcFilename), destDir); err != nil {
			return err
		}
	}
	return nil
}

func copyToDirIfNotPresent(src string, destDir string) error {
	return copyIfNotPresent(src, filepath.Join(destDir, filepath.Base(src)))
}

func copyIfNotPresent(src string, dest string) error {
	if _, err := os.Stat(dest); os.IsExist(err) {
		debugLogf("Skipping copying '%v' to '%v' because it already exists")
		return nil
	}
	debugLogf("Copying %v to %v\n", src, dest)
	in, err := os.Open(src)
	if err != nil {
		return err
	}
	defer in.Close()
	inStat, err := in.Stat()
	if err != nil {
		return err
	}
	out, err := os.OpenFile(dest, os.O_RDWR|os.O_CREATE|os.O_TRUNC, inStat.Mode())
	if err != nil {
		return err
	}
	defer out.Close()
	_, err = io.Copy(out, in)
	cerr := out.Close()
	if err != nil {
		return err
	}
	return cerr
}

func debugLogf(format string, v ...interface{}) {
	if debugEnabled {
		log.Printf(format, v...)
	}
}

func createSingleDirTarGzFile(dir string, tarFilename string) error {
	tarFile, err := os.Create(tarFilename)
	if err != nil {
		return err
	}
	defer tarFile.Close()

	gw := gzip.NewWriter(tarFile)
	defer gw.Close()
	w := tar.NewWriter(gw)
	defer w.Close()

	return filepath.Walk(dir, func(path string, info os.FileInfo, err error) error {
		if info.IsDir() {
			return nil
		}
		rel, err := filepath.Rel(dir, path)
		if err != nil {
			return err
		}
		tarPath := filepath.ToSlash(filepath.Join(filepath.Base(dir), rel))
		srcPath := filepath.Join(dir, rel)

		header, err := tar.FileInfoHeader(info, "")
		if err != nil {
			return err
		}
		header.Name = tarPath
		// Remove owner info
		header.Uname = ""
		header.Gname = ""
		header.Uid = 0
		header.Gid = 0
		if err := w.WriteHeader(header); err != nil {
			return err
		}
		src, err := os.Open(srcPath)
		if err != nil {
			return err
		}
		defer src.Close()
		_, err = io.Copy(w, src)
		return err
	})
}

func createSingleDirZipFile(dir string, zipFilename string) error {
	zipFile, err := os.Create(zipFilename)
	if err != nil {
		return err
	}
	defer zipFile.Close()

	w := zip.NewWriter(zipFile)
	defer w.Close()

	return filepath.Walk(dir, func(path string, info os.FileInfo, err error) error {
		if info.IsDir() {
			return nil
		}
		rel, err := filepath.Rel(dir, path)
		if err != nil {
			return err
		}
		zipPath := filepath.ToSlash(filepath.Join(filepath.Base(dir), rel))
		srcPath := filepath.Join(dir, rel)

		dest, err := w.Create(zipPath)
		if err != nil {
			return err
		}
		src, err := os.Open(srcPath)
		if err != nil {
			return err
		}
		defer src.Close()
		_, err = io.Copy(dest, src)
		return err
	})
}
