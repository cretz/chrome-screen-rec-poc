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

  bool SnapScreen(const QString& file);

  bool Record(const QString& file, int seconds);
};

}  // namespace attempt2

#endif  // ATTEMPT2_SCREEN_CAPTURER_H_
