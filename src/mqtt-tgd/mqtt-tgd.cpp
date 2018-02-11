#include "MqttTgD.h"

int main() {
    LOG_INFO() << "starting mqtt tgd for rtsp";
    return MqttTgD{"rtsp", "config.props"}.run();
}
