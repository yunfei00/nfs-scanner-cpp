#include "core/HardwareBringupPlan.h"

namespace NFSScanner::Core {

QString bringupStepStatusText(BringupStepStatus status)
{
    switch (status) {
    case BringupStepStatus::Pass:
        return QStringLiteral("PASS");
    case BringupStepStatus::Warn:
        return QStringLiteral("WARN");
    case BringupStepStatus::Fail:
        return QStringLiteral("FAIL");
    case BringupStepStatus::Skipped:
        return QStringLiteral("SKIP");
    case BringupStepStatus::Pending:
        break;
    }
    return QStringLiteral("PENDING");
}

QStringList HardwareBringupPlan::availableProfiles()
{
    return {QStringLiteral("mock_all"),
            QStringLiteral("motion_only_grbl"),
            QStringLiteral("spectrum_only_zna67"),
            QStringLiteral("spectrum_only_fsw"),
            QStringLiteral("spectrum_only_n9020a"),
            QStringLiteral("grbl_zna67_default"),
            QStringLiteral("grbl_fsw_default"),
            QStringLiteral("grbl_n9020a_default")};
}

HardwareBringupPlan HardwareBringupPlan::planForProfile(const QString &profileName)
{
    HardwareBringupPlan plan;
    plan.profileName = profileName;

    auto add = [&](const QString &id, const QString &title, const QString &category, bool optional = false, bool requiresHardware = false) {
        plan.steps.append(BringupStepDefinition{id, title, category, optional, requiresHardware});
    };

    add(QStringLiteral("profile_load"), QStringLiteral("加载 Profile"), QStringLiteral("profile"));
    add(QStringLiteral("motion_config"), QStringLiteral("运动平台配置检查"), QStringLiteral("motion"));
    add(QStringLiteral("motion_connect"), QStringLiteral("运动平台连接"), QStringLiteral("motion"), false, true);
    add(QStringLiteral("motion_idn"), QStringLiteral("GRBL $I 版本查询"), QStringLiteral("motion"), true, true);
    add(QStringLiteral("motion_status"), QStringLiteral("GRBL ? 状态查询"), QStringLiteral("motion"), false, true);
    add(QStringLiteral("motion_home"), QStringLiteral("GRBL Home（可选）"), QStringLiteral("motion"), true, true);
    add(QStringLiteral("motion_jog"), QStringLiteral("小步点动（可选）"), QStringLiteral("motion"), true, true);
    add(QStringLiteral("spectrum_config"), QStringLiteral("频谱仪配置检查"), QStringLiteral("spectrum"));
    add(QStringLiteral("spectrum_connect"), QStringLiteral("频谱仪 TCP 连接"), QStringLiteral("spectrum"), false, true);
    add(QStringLiteral("spectrum_idn"), QStringLiteral("*IDN?"), QStringLiteral("spectrum"));
    add(QStringLiteral("spectrum_error"), QStringLiteral("SYST:ERR?"), QStringLiteral("spectrum"));
    add(QStringLiteral("spectrum_sweep"), QStringLiteral("Single sweep + trace"), QStringLiteral("spectrum"));
    add(QStringLiteral("camera_capture"), QStringLiteral("相机 Mock capture"), QStringLiteral("camera"), true);
    add(QStringLiteral("probe_hx"), QStringLiteral("探头 Hx"), QStringLiteral("probe"), true);
    add(QStringLiteral("probe_hy"), QStringLiteral("探头 Hy"), QStringLiteral("probe"), true);
    add(QStringLiteral("prescan"), QStringLiteral("扫描前 Checklist"), QStringLiteral("scan"));
    add(QStringLiteral("report"), QStringLiteral("生成 Bring-up 报告"), QStringLiteral("report"));

    if (profileName == QStringLiteral("mock_all")) {
        for (BringupStepDefinition &step : plan.steps) {
            step.requiresHardware = false;
        }
    }

    return plan;
}

} // namespace NFSScanner::Core
