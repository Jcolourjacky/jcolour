#pragma once

// SimulatedPrinter — 无仪器 / 无打印机时的模拟后端（测试与界面演示共用）。
//
// 模型（只求「行为像打印机」，不是特性化模型）：
//   每通道一个实地色 Lab；墨膜浓度 q(u) = 浓度系数 · sat(u + 网点扩大项)，
//   sat(x) = (1 − e^{−a·x}) / (1 − e^{−a})（高墨量时趋于平台 → 有真实的墨限拐点）；
//   该通道透射比 T = (XYZ_实地 / XYZ_纸白)^q；多通道相乘再乘纸白 XYZ。
// 漂移：drift_ink() 改浓度系数（换墨批次）与网点扩大（喷头 / 环境），
//       set_paper() 改纸白（换纸批次）。
//
// 界面里凡是用到它的地方都标「模拟」，不冒充实测。

#include "print_base.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace jcolour {
namespace printbase {

struct SimChannel {
    std::string name;
    Lab    solid{50, 0, 0};    // 名义实地色（在名义纸白上）
    double shape = 3.0;        // 饱和速度 a
    double strength = 1.0;     // 浓度系数（换墨批次漂移）
    double dotGain = 0.0;      // 网点扩大附加量（0 = 名义）
};

class SimulatedPrinter {
public:
    // Epson SureColor P 系列 11 个活动通道的名义状态（PK 上机，亮光相纸）
    static SimulatedPrinter epson_p_nominal();

    SimulatedPrinter() = default;
    void addChannel(const SimChannel& c) { channels_.push_back(c); }
    void setNominalPaper(const Lab& paper) { nominalPaper_ = paper; paper_ = paper; }

    // 设备墨量（线性化之后的原始设备值 0..1）
    Lab print(const std::vector<std::pair<std::string, double>>& inks) const;
    Lab printSingle(const std::string& ch, double u) const { return print({{ch, u}}); }

    // 渐变靶实测（模拟）：steps 为原始设备输入
    ChannelRamp measureRamp(const std::string& ch, const std::vector<double>& steps) const;

    void driftInk(const std::string& ch, double strengthFactor, double dotGainDelta);
    void setPaper(const Lab& paper) { paper_ = paper; }
    Lab  paper() const { return paper_; }

    const std::vector<SimChannel>& channels() const { return channels_; }

private:
    std::vector<SimChannel> channels_;
    Lab nominalPaper_{95.5, 0.5, -2.5};
    Lab paper_{95.5, 0.5, -2.5};
};

// 标准线性化渐变靶的输入阶（0..100%，13 阶，低墨量段加密）
const std::vector<double>& default_ramp_steps();

}  // namespace printbase
}  // namespace jcolour
