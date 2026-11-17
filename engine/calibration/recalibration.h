#pragma once

// 打印机再校准（对标 GMG ColorServer「印刷机再校准」/ InstantMatch 思路）。
//
// 换纸批次或换墨批次后，不重建特性文件（ICC / G7 都不动），只用一张少量色块靶
// （每通道 4 阶 + 纸白 + 3 个灰）把机器拉回建档时的状态：
//   1. build_chart()          生成再校准靶（请求值，经当前线性化曲线打印）
//   2. reference_labs()       建档时状态：通道色块由建档渐变实测插值得到，
//                             灰块用建档时记录的 grayReference（若有）
//   3. compute_recalibration  实测 vs 参考 → 每通道新的线性化曲线
//                             （以建档响应形状 × 稀疏实测比例修正，墨限不变）
//   4. apply_recalibration    只替换曲线；墨限、TIL、渐变参考、灰参考都不变
//   5. evaluate()             校准前后偏差（ΔE00 平均 / 最大，分通道）

#include "print_base.h"

#include <string>
#include <utility>
#include <vector>

namespace jcolour {
namespace recal {

using printbase::Lab;

struct ChartPatch {
    std::string id;          // "PAPER" / "C-50" / "GRAY-50"
    std::string channel;     // 通道名；纸白 = ""；灰 = "GRAY"
    double      request = 0; // 请求值 0..1（线性化前的「线性空间」值）
};

struct ChartSpec {
    std::vector<std::string> channels;
    std::vector<double> steps{0.25, 0.50, 0.75, 1.00};
    std::vector<double> grays{0.25, 0.50, 0.75};
    // 灰块配方：通道 → 权重（请求值 = 权重 × 灰阶）。默认 C/VM/Y 三色灰。
    std::vector<std::pair<std::string, double>> grayRecipe{{"C", 1.0}, {"VM", 0.82}, {"Y", 0.86}};
};

std::vector<ChartPatch> build_chart(const ChartSpec& spec);

// 某色块经当前曲线后的设备墨量（打印用）：{通道, 设备值}
std::vector<std::pair<std::string, double>>
device_inks(const ChartPatch& p, const printbase::PrintBase& base, const ChartSpec& spec);

// 参考 Lab；valid[i] = false 表示该色块没有建档基准（灰块未记录时）
struct Reference {
    std::vector<Lab>  labs;
    std::vector<bool> valid;
};
Reference reference_labs(const printbase::PrintBase& base, const std::vector<ChartPatch>& chart);

struct PatchDelta {
    std::string id, channel;
    double request = 0;
    Lab    reference{0, 0, 0}, measured{0, 0, 0};
    double de00 = 0;
    bool   hasReference = false;
};

struct Summary {
    double avg = 0, max = 0;   // 仅统计有参考的色块
    int    count = 0;
};

struct Evaluation {
    std::vector<PatchDelta> patches;
    Summary all, channelsOnly, grays;
};
Evaluation evaluate(const std::vector<ChartPatch>& chart, const Reference& ref,
                    const std::vector<Lab>& measured);

struct ChannelCorrection {
    std::string channel;
    printbase::LinearizationCurve oldCurve, newCurve;
    double shiftAtMid = 0;      // t=0.5 处设备值变化（新 − 旧）
    double maxShift = 0;        // 全曲线最大 |变化|
    bool   hitLimit = false;    // 需要超过墨限才能拉回（墨变淡太多）→ 只能部分校正
    Summary before;             // 该通道色块校准前 ΔE00
};

struct RecalResult {
    Evaluation before;
    std::vector<ChannelCorrection> channels;
    std::vector<std::string> warnings;   // 代码：PAPER_SHIFT:<ΔE> / HIT_LIMIT:<ch> / NO_REFERENCE_GRAY
    bool ok = false;
};

RecalResult compute_recalibration(const printbase::PrintBase& base,
                                  const std::vector<ChartPatch>& chart,
                                  const std::vector<Lab>& measured);

// 只替换线性化曲线（ICC / G7 / 墨限 / TIL 不动）
printbase::PrintBase apply_recalibration(const printbase::PrintBase& base,
                                         const RecalResult& r);

}  // namespace recal
}  // namespace jcolour
