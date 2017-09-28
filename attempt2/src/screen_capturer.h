#ifndef ATTEMPT2_SCREEN_CAPTURER_H_
#define ATTEMPT2_SCREEN_CAPTURER_H_

#include <QtCore>

namespace attempt2 {

class ScreenCapturer : public QObject {
  Q_OBJECT
 public:
  const uint kRecordBitRate = 16000000;
  const uint kFramesPerSecond = 60;

  explicit ScreenCapturer(QObject* parent = nullptr);

  bool CreateBackgroundDesktop(const QString& name);
  bool OpenChrome(const QString& exe, const QStringList& args);
  bool CloseChrome();
  bool SnapScreen(const QString& file);
  bool Record(const QString& file, int seconds);

 private:
  void* chrome_process_ = nullptr;
};

}  // namespace attempt2

#endif  // ATTEMPT2_SCREEN_CAPTURER_H_
