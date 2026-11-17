#include "sbom.h"

#include "argyll.h"
#include "i1pro3_driver.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QtGlobal>

#include <lcms2.h>
#include <sqlite3.h>
#include <tiffio.h>

namespace jcolour::sbom {

namespace {

QString lcmsVersion() {
    const int v = cmsGetEncodedCMMversion();   // 例：2160 → 2.16
    return QStringLiteral("%1.%2").arg(v / 1000).arg((v % 1000) / 10);
}

QString tiffVersion() {
    // "LIBTIFF, Version 4.7.0\nCopyright …"
    const QString s = QString::fromLatin1(TIFFGetVersion());
    const auto m = QRegularExpression(QStringLiteral("Version\\s+([0-9][0-9.]*)")).match(s);
    return m.hasMatch() ? m.captured(1) : QString();
}

QString purl(const Component& c) {
    QString n = c.name.toLower();
    n.replace(QRegularExpression(QStringLiteral("[^a-z0-9.+-]+")), QStringLiteral("-"));
    return QStringLiteral("pkg:generic/%1%2").arg(n, c.version.isEmpty() ? QString() : "@" + c.version);
}

}  // namespace

QVector<Component> collect(bool probeExternal) {
    QVector<Component> v;

    v.push_back({QStringLiteral("Qt"), QString::fromLatin1(qVersion()), QStringLiteral("LGPL-3.0-only"),
                 QStringLiteral("The Qt Company Ltd."), QStringLiteral("https://www.qt.io/"),
                 QStringLiteral("界面框架（Core / Gui / Widgets / PrintSupport）"),
                 QStringLiteral("动态链接"), QStringLiteral("required"),
                 QStringLiteral("Copyright (C) The Qt Company Ltd. and other contributors."),
                 QStringLiteral("按 LGPL-3.0 以动态库方式使用；用户可替换 Qt 库重新链接，Qt 源码见 download.qt.io。")});

    v.push_back({QStringLiteral("Little CMS"), lcmsVersion(), QStringLiteral("MIT"),
                 QStringLiteral("Marti Maria Saguer"), QStringLiteral("https://www.littlecms.com/"),
                 QStringLiteral("色彩管理引擎（ICC 变换、软打样、色域外映射）"),
                 QStringLiteral("动态链接"), QStringLiteral("required"),
                 QStringLiteral("Copyright (c) 1998-2024 Marti Maria Saguer"), QString()});

    v.push_back({QStringLiteral("libtiff"), tiffVersion(), QStringLiteral("libtiff"),
                 QStringLiteral("libtiff project"), QStringLiteral("https://libtiff.gitlab.io/libtiff/"),
                 QStringLiteral("16 位 TIFF 读写"), QStringLiteral("动态链接"), QStringLiteral("required"),
                 QStringLiteral("Copyright (c) 1988-1997 Sam Leffler; Copyright (c) 1991-1997 Silicon Graphics, Inc."),
                 QString()});

    v.push_back({QStringLiteral("SQLite"), QString::fromLatin1(sqlite3_libversion()), QStringLiteral("blessing"),
                 QStringLiteral("SQLite Consortium"), QStringLiteral("https://sqlite.org/"),
                 QStringLiteral("纸张状态库（打印环境 / 校准参数）"), QStringLiteral("动态链接"),
                 QStringLiteral("required"), QStringLiteral("Public domain"), QString()});

    Component argyll{QStringLiteral("ArgyllCMS"), QString(), QStringLiteral("AGPL-3.0-only"),
                     QStringLiteral("Graeme W. Gill"), QStringLiteral("https://www.argyllcms.com/"),
                     QStringLiteral("本机 ICC 建档工具（targen / colprof / profcheck）"),
                     QStringLiteral("独立进程（未链接）"), QStringLiteral("optional"),
                     QStringLiteral("Copyright (c) Graeme W. Gill"),
                     QStringLiteral("jcolour 不链接 Argyll 代码，只在用户本机已安装时以子进程调用（ADR-07）。")};
    if (probeExternal) argyll.version = QString::fromStdString(jcolour::argyll::version());
    v.push_back(argyll);

    Component gp{QStringLiteral("Gutenprint"), QString(), QStringLiteral("GPL-2.0-or-later"),
                 QStringLiteral("Gutenprint project"), QStringLiteral("https://gimp-print.sourceforge.io/"),
                 QStringLiteral("ESC/P2 渲染后端（jcolour-stp-backend）"),
                 QStringLiteral("独立进程（未链接）"), QStringLiteral("optional"),
                 QStringLiteral("Copyright (c) Gutenprint contributors"),
                 QStringLiteral("GPL 代码只链接进独立的 jcolour-stp-backend 进程，主程序经进程边界通信；未装 Gutenprint 时为空壳。")};
    v.push_back(gp);

    Component i1{QStringLiteral("X-Rite i1Pro3 SDK"), QString(), QStringLiteral("LicenseRef-X-Rite-SDK"),
                 QStringLiteral("X-Rite, Incorporated"), QStringLiteral("https://www.xrite.com/"),
                 QStringLiteral("i1Pro3 分光光度计驱动"), QStringLiteral("运行时加载"), QStringLiteral("optional"),
                 QStringLiteral("Copyright (c) X-Rite, Incorporated"),
                 QStringLiteral("运行时 dlopen 加载，按 X-Rite SDK 许可使用；未安装时 jcolour 退回模拟 / Argyll 测量。")};
    if (probeExternal) {
        auto& d = jcolour::i1sdk::I1Pro3Driver::instance();
        if (d.available()) i1.version = QStringLiteral("已加载");
    }
    v.push_back(i1);

    for (const auto& [name, purpose] : {
             std::pair{QStringLiteral("Apple CoreFoundation / IOKit"), QStringLiteral("USB 仪器访问")},
             std::pair{QStringLiteral("Apple OpenGL"), QStringLiteral("预览绘制")},
             std::pair{QStringLiteral("CUPS (libcups)"), QStringLiteral("系统打印队列")},
             std::pair{QStringLiteral("DNS-SD (Bonjour)"), QStringLiteral("网络打印机发现")}}) {
        v.push_back({name, QString(), QStringLiteral("LicenseRef-Apple-System"), QStringLiteral("Apple Inc."),
                     QString(), purpose, QStringLiteral("系统组件"), QStringLiteral("excluded"),
                     QString(), QStringLiteral("随 macOS 提供，不随 jcolour 分发。")});
    }
    return v;
}

QByteArray toCycloneDx(const QVector<Component>& comps, const BuildInfo& info,
                       const QDateTime& timestamp, const QString& serialUuid) {
    QJsonObject root;
    root["bomFormat"] = QStringLiteral("CycloneDX");
    root["specVersion"] = QStringLiteral("1.5");
    root["serialNumber"] = QStringLiteral("urn:uuid:%1").arg(serialUuid);
    root["version"] = 1;

    QJsonObject app;
    app["type"] = QStringLiteral("application");
    app["name"] = info.appName;
    app["version"] = info.appVersion;
    app["supplier"] = QJsonObject{{"name", info.supplier}};
    app["bom-ref"] = QStringLiteral("app:%1").arg(info.appName);
    QJsonObject meta;
    meta["timestamp"] = timestamp.toUTC().toString(Qt::ISODate);
    meta["component"] = app;
    meta["tools"] = QJsonObject{{"components", QJsonArray{QJsonObject{
        {"type", "application"}, {"name", info.appName + " SBOM exporter"}, {"version", info.appVersion}}}}};
    root["metadata"] = meta;

    QJsonArray list, deps;
    QJsonArray appDeps;
    for (const auto& c : comps) {
        QJsonObject o;
        o["type"] = c.linkage == QStringLiteral("独立进程（未链接）") ? QStringLiteral("application")
                                                                   : QStringLiteral("library");
        o["name"] = c.name;
        if (!c.version.isEmpty()) o["version"] = c.version;
        o["scope"] = c.scope;
        o["supplier"] = QJsonObject{{"name", c.supplier}};
        o["purl"] = purl(c);
        const QString ref = QStringLiteral("dep:%1").arg(c.name);
        o["bom-ref"] = ref;
        if (!c.copyright.isEmpty()) o["copyright"] = c.copyright;
        if (!c.purpose.isEmpty()) o["description"] = c.purpose;
        QJsonObject lic;
        if (c.license.startsWith(QStringLiteral("LicenseRef-")))
            lic["name"] = c.license;
        else
            lic["id"] = c.license;
        o["licenses"] = QJsonArray{QJsonObject{{"license", lic}}};
        if (!c.url.isEmpty())
            o["externalReferences"] = QJsonArray{QJsonObject{{"type", "website"}, {"url", c.url}}};
        QJsonArray props{QJsonObject{{"name", "jcolour:linkage"}, {"value", c.linkage}}};
        if (!c.note.isEmpty()) props.append(QJsonObject{{"name", "jcolour:note"}, {"value", c.note}});
        o["properties"] = props;
        list.append(o);
        appDeps.append(ref);
    }
    root["components"] = list;
    deps.append(QJsonObject{{"ref", app["bom-ref"]}, {"dependsOn", appDeps}});
    root["dependencies"] = deps;
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

QString thirdPartyNotices(const QVector<Component>& comps, const BuildInfo& info) {
    QString t;
    t += QStringLiteral("%1 %2 — 第三方软件声明\n").arg(info.appName, info.appVersion);
    t += QStringLiteral("本软件使用或调用了下列第三方组件。各组件的版权归其权利人所有，按各自许可证使用。\n\n");
    for (const auto& c : comps) {
        t += QStringLiteral("■ %1%2\n").arg(c.name, c.version.isEmpty() ? QString() : QStringLiteral(" ") + c.version);
        t += QStringLiteral("  许可证：%1　使用方式：%2\n").arg(c.license, c.linkage);
        if (!c.copyright.isEmpty()) t += QStringLiteral("  %1\n").arg(c.copyright);
        if (!c.url.isEmpty()) t += QStringLiteral("  %1\n").arg(c.url);
        if (!c.note.isEmpty()) t += QStringLiteral("  说明：%1\n").arg(c.note);
        t += QLatin1Char('\n');
    }
    t += QStringLiteral(
        "许可证全文：MIT、libtiff、LGPL-3.0、GPL-2.0、AGPL-3.0 见 https://spdx.org/licenses/ 对应条目；"
        "SQLite 为公有领域（blessing）。\n"
        "界面图标按 Lucide 线条图标风格自绘（未引入 Lucide 资源文件；Lucide 为 ISC 许可）。\n");
    return t;
}

}  // namespace jcolour::sbom
