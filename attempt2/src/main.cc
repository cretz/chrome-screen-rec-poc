#include <QtGui>

#include "screen_capturer.h"

int main(int argc, char *argv[]) {
  QCoreApplication a(argc, argv);

  ::attempt2::ScreenCapturer cap;

  if (a.arguments().last() == "--restart-on-new-desktop") {
    qDebug() << "Creating background desktop...";
    if (!cap.CreateBackgroundDesktop("attempt2")) return 1;
    QProcess child;
    child.setProcessChannelMode(QProcess::ForwardedChannels);
    child.setCreateProcessArgumentsModifier(
          [] (QProcess::CreateProcessArguments* args) {
      std::wstring desktop_name = L"attempt2";
      std::vector<wchar_t> desktop_name_vec(desktop_name.begin(),
                                            desktop_name.end());
      desktop_name_vec.push_back(0);
      args->startupInfo->lpDesktop = desktop_name_vec.data();
    });
    qDebug() << "Starting child...";
    child.start(a.applicationFilePath());
    if (!child.waitForFinished()) qDebug() << "Child done fail";
    qDebug() << "Child done!";
    return 0;
  }

  auto success = cap.OpenChrome(
      "C:\\Program Files (x86)\\Google\\Chrome Dev\\Application\\chrome.exe",
      {
        "--disable-infobars",
        "--no-sandbox",
        "--disable-setuid-sandbox",
        "--incognito",
        "https://www.youtube.com/embed/0vrdgDdPApQ?"
          "autoplay=1&modestbranding=1&showinfo=0&controls=0"
      });
  qDebug() << "Process create success:" << success;
  if (!success) return 1;

  // success = cap.SnapScreen("test.png");
  // qDebug() << "Snap success:" << success;

  success = cap.Record("test.mpg", 30);
  qDebug() << "Record success:" << success;

  cap.CloseChrome();

  // return a.exec();
  return 0;
}
