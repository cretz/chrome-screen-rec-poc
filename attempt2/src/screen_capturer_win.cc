#include "screen_capturer.h"

#include <codecapi.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <evr.h>
#include <mfapi.h>
#include <Mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <QtWinExtras>
#include <tchar.h>
#include <time.h>
#include <Windows.h>

namespace attempt2 {

bool ScreenCapturer::CreateBackgroundDesktop(const QString& name) {
  auto fail = [](const QString& text) -> bool {
    qCritical() << "Create desktop failed:" << text;
    return false;
  };
  auto desk = CreateDesktop(name.toStdWString().c_str(),
                            nullptr,
                            nullptr,
                            DF_ALLOWOTHERACCOUNTHOOK,
                            GENERIC_ALL,
                            nullptr);
  if (!desk) return fail("Desktop create failed");
  if (!SetThreadDesktop(desk)) return fail("Set thread desktop fail");
  return true;
}

bool ScreenCapturer::OpenChrome(const QString& exe, const QStringList& args) {
  auto fail = [](const QString& text) -> bool {
    qCritical() << "Open failed:" << text;
    return false;
  };

  if (chrome_process_) return fail("Chrome already open");

  auto cmd = (exe + " " + args.join(' ')).toStdWString();
  std::vector<wchar_t> cmd_vec(cmd.begin(), cmd.end());
  cmd_vec.push_back(0);
  STARTUPINFO startup_info = { sizeof(startup_info) };
  PROCESS_INFORMATION process_info = {};
  auto res = CreateProcess(nullptr, cmd_vec.data(), nullptr, nullptr, false,
                           0, nullptr, nullptr, &startup_info, &process_info);
  if (!res) return fail("Failed starting chrome");
  chrome_process_ = process_info.hProcess;
  return true;
}

bool ScreenCapturer::CloseChrome() {
  auto fail = [](const QString& text) -> bool {
    qCritical() << "Close failed:" << text;
    return false;
  };

  if (!chrome_process_) return fail("Chrome not open");
  auto res = TerminateProcess(chrome_process_, 1);
  if (!res) return fail("Process termination failed");
  chrome_process_ = nullptr;

  return true;
}

bool ScreenCapturer::SnapScreen(const QString& file) {
  // Grab the desktop DC
  auto screen_dc = GetDC(nullptr);

  // Create in-memory DC
  auto in_mem_dc = CreateCompatibleDC(screen_dc);

  // Get width and height in pixels
  auto width = GetDeviceCaps(screen_dc, HORZRES);
  auto height = GetDeviceCaps(screen_dc, VERTRES);

  // Create bitmap and select into DC
  auto bitmap = CreateCompatibleBitmap(screen_dc, width, height);
  auto old_obj = SelectObject(in_mem_dc, bitmap);

  // Copy screen
  auto success = BitBlt(in_mem_dc, 0, 0, width, height,
                        screen_dc, 0, 0, SRCCOPY);
  if (success) {
    bitmap = (HBITMAP) SelectObject(in_mem_dc, old_obj);
    auto image = QtWin::imageFromHBITMAP(in_mem_dc, bitmap, width, height);
    success = image.save(file);
  }

  // Clean up
  DeleteDC(in_mem_dc);
  ReleaseDC(nullptr, screen_dc);
  DeleteObject(bitmap);

  return success;
}

bool ScreenCapturer::Record(const QString& file, int seconds) {
  // Startup mf
  auto res = MFStartup(MF_VERSION);
  if (FAILED(res)) {
    qCritical() << "Startup failed" << QtWin::stringFromHresult(res);
    return false;
  }

  // Release in reverse please
  // TODO(cretz): Release references more eagerly instead of at the end
  QList<IUnknown*> to_release;
  // Returns false for easy return
  auto do_release = [&to_release](
      const QString& error_subj = QString(),
      HRESULT res = S_OK) -> bool {
    if (!error_subj.isNull()) {
      qCritical() << "Error on" << error_subj << QtWin::stringFromHresult(res);
    }
    for (int i = to_release.size() - 1; i >= 0; i--) {
      if (to_release[i]) to_release[i]->Release();
    }
    MFShutdown();
    return false;
  };

  // Grab desktop info
  HWND desktop = GetDesktopWindow();
  RECT desktop_size;
  GetWindowRect(desktop, &desktop_size);

  // Create device
  ID3D11Device* d3d_device;
  ID3D11DeviceContext* device_ctx;
  res = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, 0, 0,
                          D3D11_SDK_VERSION, &d3d_device, nullptr, &device_ctx);
  if (FAILED(res)) return do_release("Device creation", res);
  to_release << d3d_device;
  IDXGIDevice* dx_device;
  res = d3d_device->QueryInterface(&dx_device);
  if (FAILED(res)) return do_release("Device query", res);
  to_release << dx_device;

  // Grab adapter
  IDXGIAdapter* adapter;
  res = dx_device->GetAdapter(&adapter);
  if (FAILED(res)) return do_release("Adapter fetch", res);
  to_release << adapter;
  DXGI_ADAPTER_DESC adapter_desc;
  if (SUCCEEDED(adapter->GetDesc(&adapter_desc))) {
    qDebug() << "Got adapter:" <<
                QString::fromWCharArray(adapter_desc.Description);
  }

  // Grab first output
  IDXGIOutput* output;
  res = adapter->EnumOutputs(0, &output);
  if (FAILED(res)) return do_release("Output find", res);
  to_release << output;
  DXGI_OUTPUT_DESC output_desc;
  if (SUCCEEDED(output->GetDesc(&output_desc))) {
    qDebug() << "Got output:" <<
                QString::fromWCharArray(output_desc.DeviceName);
  }
  IDXGIOutput1* output_1;
  res = output->QueryInterface(&output_1);
  if (FAILED(res)) return do_release("Iface query", res);
  to_release << output_1;

  // Create device manager
  IMFDXGIDeviceManager* manager;
  UINT manager_reset_token;
  res = MFCreateDXGIDeviceManager(&manager_reset_token, &manager);
  if (FAILED(res)) return do_release("Create device manager", res);
  to_release << manager;
  res = manager->ResetDevice(d3d_device, manager_reset_token);
  if (FAILED(res)) return do_release("Reset device", res);

  // Create sink writer
  IMFAttributes* attrs;
  res = MFCreateAttributes(&attrs, 4);
  if (FAILED(res)) return do_release("Create attrs", res);
  to_release << attrs;
  res = attrs->SetGUID(MF_TRANSCODE_CONTAINERTYPE,
                       MFTranscodeContainerType_FMPEG4);
  if (FAILED(res)) return do_release("Set MF_TRANSCODE_CONTAINERTYPE", res);
  res = attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, 1);
  if (FAILED(res)) {
    return do_release("Set MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS", res);
  }
  res = attrs->SetUINT32(MF_LOW_LATENCY, 1);
  if (FAILED(res)) return do_release("Set MF_LOW_LATENCY", res);
  res = attrs->SetUnknown(MF_SINK_WRITER_D3D_MANAGER, manager);
  if (FAILED(res)) return do_release("Set MF_SINK_WRITER_D3D_MANAGER", res);
  IMFSinkWriter* writer;
  res = MFCreateSinkWriterFromURL(reinterpret_cast<LPCWSTR>(file.utf16()),
                                  nullptr, attrs, &writer);
  if (FAILED(res)) return do_release("Create writer", res);
  to_release << writer;

  // Add output stream type
  IMFMediaType* media_type_out;
  res = MFCreateMediaType(&media_type_out);
  if (FAILED(res)) return do_release("Create out media type", res);
  to_release << media_type_out;
  res = media_type_out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  if (FAILED(res)) return do_release("Set out MF_MT_MAJOR_TYPE", res);
  res = media_type_out->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
  if (FAILED(res)) return do_release("Set out MF_MT_SUBTYPE", res);
  res = media_type_out->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Base);
  if (FAILED(res)) return do_release("Set out MF_MT_MPEG2_PROFILE", res);
  res = media_type_out->SetUINT32(MF_MT_AVG_BITRATE, kRecordBitRate);
  if (FAILED(res)) return do_release("Set out MF_MT_AVG_BITRATE", res);
  res = media_type_out->SetUINT32(MF_MT_INTERLACE_MODE,
                                  MFVideoInterlace_Progressive);
  if (FAILED(res)) return do_release("Set out MF_MT_INTERLACE_MODE", res);
  res = MFSetAttributeSize(media_type_out, MF_MT_FRAME_SIZE,
                           desktop_size.right, desktop_size.bottom);
  if (FAILED(res)) return do_release("Set out MF_MT_FRAME_SIZE", res);
  res = MFSetAttributeRatio(media_type_out, MF_MT_FRAME_RATE,
                            kFramesPerSecond, 1);
  if (FAILED(res)) return do_release("Set out MF_MT_FRAME_RATE", res);
  res = MFSetAttributeRatio(media_type_out, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
  if (FAILED(res)) return do_release("Set out MF_MT_PIXEL_ASPECT_RATIO", res);
  DWORD stream_index;
  res = writer->AddStream(media_type_out, &stream_index);
  if (FAILED(res)) return do_release("Add stream", res);

  // Set input stream type
  IMFMediaType* media_type_in;
  res = MFCreateMediaType(&media_type_in);
  if (FAILED(res)) return do_release("Create in media type", res);
  to_release << media_type_in;
  res = media_type_in->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  if (FAILED(res)) return do_release("Set in MF_MT_MAJOR_TYPE", res);
  res = media_type_in->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_ARGB32);
  if (FAILED(res)) return do_release("Set in MF_MT_SUBTYPE", res);
  res = media_type_in->SetUINT32(MF_MT_INTERLACE_MODE,
                                 MFVideoInterlace_Progressive);
  if (FAILED(res)) return do_release("Set in MF_MT_INTERLACE_MODE", res);
  res = MFSetAttributeSize(media_type_in, MF_MT_FRAME_SIZE,
                           desktop_size.right, desktop_size.bottom);
  if (FAILED(res)) return do_release("Set in MF_MT_FRAME_SIZE", res);
  res = MFSetAttributeRatio(media_type_in, MF_MT_FRAME_RATE,
                            kFramesPerSecond, 1);
  if (FAILED(res)) return do_release("Set in MF_MT_FRAME_RATE", res);
  res = MFSetAttributeRatio(media_type_in, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
  if (FAILED(res)) return do_release("Set in MF_MT_PIXEL_ASPECT_RATIO", res);
  res = writer->SetInputMediaType(stream_index, media_type_in, 0);
  if (FAILED(res)) return do_release("Set in media type", res);

  // Begin writing
  res = writer->BeginWriting();
  if (FAILED(res)) return do_release("Begin writing", res);

  // Begin output duplication
  IDXGIOutputDuplication* out_dup;
  res = output_1->DuplicateOutput(d3d_device, &out_dup);
  if (FAILED(res)) return do_release("Duplicate output", res);

  // Create temporary texture we write each frame to
  ID3D11Texture2D* temp_texture;
  CD3D11_TEXTURE2D_DESC temp_texture_desc(
        DXGI_FORMAT_B8G8R8A8_UNORM, desktop_size.right, desktop_size.bottom,
        1, 1, D3D11_BIND_RENDER_TARGET);
  res = d3d_device->CreateTexture2D(&temp_texture_desc, nullptr, &temp_texture);
  if (FAILED(res)) return do_release("Create temp texture", res);
  to_release << temp_texture;

  // Pre-calc frame duration
  // TODO(cretz): This is unused, should I use it for SetSampleDuration?
  UINT64 frame_duration;
  res = MFFrameRateToAverageTimePerFrame(kFramesPerSecond, 1, &frame_duration);
  if (FAILED(res)) return do_release("Calc frame duration", res);

  // Count information to know ns diff
  LARGE_INTEGER counts_per_sec = {};
  QueryPerformanceFrequency(&counts_per_sec);
  double counts_per_deca_ns =
      static_cast<double>(counts_per_sec.QuadPart) / 10000000.0;
  LARGE_INTEGER prev_count = {};
  LARGE_INTEGER curr_count = {};
  QueryPerformanceCounter(&prev_count);
  // Diff since last in 100-ns vals
  LONGLONG deca_ns_since_start = 0;
  LONGLONG deca_ns_diff = 0;

  auto start = time(nullptr);
  // Only run for X seconds
  while (difftime(time(nullptr), start) < seconds) {
    // Grab frame surface
    DXGI_OUTDUPL_FRAME_INFO frame_info;
    IDXGIResource* frame;
    res = out_dup->AcquireNextFrame(INFINITE, &frame_info, &frame);
    if (FAILED(res)) return do_release("Acquire frame", res);
    IDXGISurface* surface;
    res = frame->QueryInterface(&surface);
    if (FAILED(res)) return do_release("Query surface", res);
    ID3D11Texture2D* texture;
    res = surface->QueryInterface(&texture);
    if (FAILED(res)) return do_release("Query texture", res);

    // Copy the texture and release the frame
    device_ctx->CopyResource(temp_texture, texture);
    out_dup->ReleaseFrame();
    texture->Release();
    surface->Release();

    // TODO(cretz): thread the rest of this

    // Create the sample
    IMFSample* sample;
    res = MFCreateSample(&sample);
    if (FAILED(res)) return do_release("Create sample", res);
    IMFMediaBuffer* media_buffer;
    res = MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), temp_texture, 0,
                                    false, &media_buffer);
    if (FAILED(res)) return do_release("Create buffer", res);
    IMF2DBuffer* twod_buffer;
    res = media_buffer->QueryInterface(&twod_buffer);
    if (FAILED(res)) return do_release("Query 2d buffer", res);

    DWORD length = 0;
    res = twod_buffer->GetContiguousLength(&length);
    if (FAILED(res)) return do_release("Get contiguous length", res);
    twod_buffer->Release();
    res = media_buffer->SetCurrentLength(length);
    if (FAILED(res)) return do_release("Set current length", res);
    res = sample->AddBuffer(media_buffer);
    if (FAILED(res)) return do_release("Add buffer", res);
    media_buffer->Release();
    sample->SetSampleDuration(deca_ns_diff);
    sample->SetSampleTime(deca_ns_since_start);

    // Write the sample
    res = writer->WriteSample(stream_index, sample);
    sample->Release();
    if (FAILED(res)) return do_release("Write sample", res);

    // Set diffs for next time
    QueryPerformanceCounter(&curr_count);
    deca_ns_diff = LONGLONG((curr_count.QuadPart - prev_count.QuadPart) /
                            counts_per_deca_ns);
    deca_ns_since_start += deca_ns_diff;
    prev_count = curr_count;
  }

  res = writer->Finalize();
  if (FAILED(res)) return do_release("Finalize writer", res);

  do_release();
  return true;
}

}  // namespace attempt2
