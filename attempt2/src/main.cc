#include <QtGui>

#include "screen_capturer.h"

int main(int argc, char *argv[]) {
  QCoreApplication a(argc, argv);

  ::attempt2::ScreenCapturer cap;
  auto success = cap.SnapScreen("test.png");
  qDebug() << "Snap success: " << success;

  success = cap.Record("test.mpg", 15);
  qDebug() << "Record success: " << success;

  // return a.exec();
}
