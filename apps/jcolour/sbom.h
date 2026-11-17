#pragma once

// 软件物料清单（SBOM）与第三方声明（对标 GMG ColorProof GO 在「关于」里提供 SBOM）。
//
// 组件表在运行时取真实版本（Qt / Little CMS / libtiff / SQLite 由库自身 API 报告，
// ArgyllCMS / i1Pro3 SDK 运行时探测），导出为 CycloneDX 1.5 JSON——三款软件
// （ProofTrack · gcolour · jcolour）统一用 CycloneDX 1.5，企业合规审查直接导入。

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QVector>

namespace jcolour::sbom {

struct Component {
    QString name;
    QString version;       // 运行时报告；空 = 未检测到 / 未安装
    QString license;       // SPDX 标识
    QString supplier;
    QString url;
    QString purpose;       // 在 jcolour 里做什么（中文）
    QString linkage;       // 静态链接 / 动态链接 / 运行时加载 / 独立进程（未链接）/ 系统组件
    QString scope;         // required / optional / excluded（系统组件，不随软件分发）
    QString copyright;
    QString note;          // 合规说明（中文）
};

struct BuildInfo {
    QString appName = QStringLiteral("jcolour");
    QString appVersion;
    QString supplier = QStringLiteral("ColourChecker");
};

// probeExternal = true 时会探测 ArgyllCMS（启动子进程取版本）与 i1Pro3 框架（dlopen）
QVector<Component> collect(bool probeExternal);

// CycloneDX 1.5 JSON（serial 为 urn:uuid）
QByteArray toCycloneDx(const QVector<Component>& comps, const BuildInfo& info,
                       const QDateTime& timestamp, const QString& serialUuid);

// 第三方声明（纯文本，可放进「关于」与随包 THIRD_PARTY_NOTICES.txt）
QString thirdPartyNotices(const QVector<Component>& comps, const BuildInfo& info);

}  // namespace jcolour::sbom
