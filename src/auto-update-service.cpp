#if defined(HAVE_SPARKLE_SUPPORT)
#include <QSettings>

#ifdef Q_OS_WIN32
    #include <winsparkle.h>
#else
    #include "mac-sparkle-support.h"
#endif

#include "i18n.h"
#include "api/requests.h"
#include "seadrive-gui.h"
#include "utils/utils.h"

#include "auto-update-service.h"

SINGLETON_IMPL(AutoUpdateService)

namespace
{
#ifdef Q_OS_WIN32
    const char *kSparkleAppcastURI = "https://www.seafile.com/api/client-updates/seadrive-client-windows/appcast.xml";
    const char *kSparkleAppcastURIForCN = "https://www.seafile.com/api/client-updates/seadrive-client-windows-cn/appcast.xml";
    const char *kWinSparkleRegistryPath = "SOFTWARE\\Seafile\\Seafile Drive Client\\WinSparkle";
#else
    const char *kSparkleAppcastURI = "https://www.seafile.com/api/client-updates/seadrive-client-mac/appcast.xml";
    const char *kSparkleAppcastURIForCN = "https://www.seafile.com/api/client-updates/seadrive-client-mac-cn/appcast.xml";
#endif
    const char *kSparkleAlreadyEnableUpdateByDefault = "SparkleAlreadyEnableUpdateByDefault";
    const char *const kPreconfigureEnableAutoUpdate = "PreconfigureEnableAutoUpdate";

QString getAppcastURI() {
    QString url_from_env = qgetenv("SEADRIVE_CLIENT_APPCAST_URI");
    if (!url_from_env.isEmpty()) {
        qWarning(
            "sparkle: using app cast url from SEADRIVE_CLIENT_APPCAST_URI: "
            "%s",
            url_from_env.toUtf8().data());
        return url_from_env;
    }
    return I18NHelper::getInstance()->isTargetLanguage("zh-CN") ? kSparkleAppcastURIForCN : kSparkleAppcastURI;
}

} // namespace

// Virtual base class for windows/mac
class AutoUpdateAdapter {
public:
    virtual void prepare() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void checkNow() = 0;
    virtual bool autoUpdateEnabled() = 0;
    virtual void setAutoUpdateEnabled(bool enabled) = 0;
};

#ifdef Q_OS_WIN32
class WindowsAutoUpdateAdapter: public AutoUpdateAdapter {
public:
    void prepare() {
        // Note that @param path is relative to HKCU/HKLM root
        // and the root is not part of it. For example:
        // @code
        //     win_sparkle_set_registry_path("Software\\My App\\Updates");
        // @endcode
        win_sparkle_set_registry_path(kWinSparkleRegistryPath);
        win_sparkle_set_appcast_url(getAppcastURI().toUtf8().data());
        win_sparkle_set_app_details(
            L"Seafile",
            L"Seafile Drive Client",
            QString(STRINGIZE(SEADRIVE_GUI_VERSION)).toStdWString().c_str());
    }

    void start() {
        win_sparkle_init();
    }

    void stop() {
        win_sparkle_cleanup();
    }

    void checkNow() {
        win_sparkle_check_update_with_ui();
    }

    bool autoUpdateEnabled() {
        // qWarning() << "autoUpdateEnabled =" << win_sparkle_get_automatic_check_for_updates();
        return win_sparkle_get_automatic_check_for_updates();
    }

    void setAutoUpdateEnabled(bool enabled) {
        win_sparkle_set_automatic_check_for_updates(enabled ? 1 : 0);
    }
};
#elif defined(Q_OS_MAC)
class MacAutoUpdateAdapter: public AutoUpdateAdapter {
public:
    void prepare() {
        SparkleHelper::setFeedURL(getAppcastURI().toUtf8().data());
    }

    void start() {
    }

    void stop() {
    }

    void checkNow() {
        SparkleHelper::checkForUpdate();
    }

    bool autoUpdateEnabled() {
        return SparkleHelper::autoUpdateEnabled();
    }

    void setAutoUpdateEnabled(bool enabled) {
        SparkleHelper::setAutoUpdateEnabled(enabled);
    }
};
#endif


AutoUpdateService::AutoUpdateService(QObject *parent) : QObject(parent)
{
#ifdef Q_OS_WIN32
    adapter_ = new WindowsAutoUpdateAdapter;
#else
    adapter_ = new MacAutoUpdateAdapter;
#endif
}

void AutoUpdateService::start()
{
    adapter_->prepare();
    enableUpdateByDefault();
    adapter_->start();
}

void AutoUpdateService::enableUpdateByDefault() {
    // Enable auto update check by default.
    QSettings settings;
    settings.beginGroup("Misc");
    bool already_enable_update_by_default = settings.value(kSparkleAlreadyEnableUpdateByDefault, false).toBool();

#if defined(Q_OS_WIN32)
    QString enable_auto_update = gui->readPreconfigureExpandedString(kPreconfigureEnableAutoUpdate);

    if (enable_auto_update == "1") {
        setAutoUpdateEnabled(true);
    } else if (enable_auto_update == "0") {
        setAutoUpdateEnabled(false);
    } else if (!already_enable_update_by_default && enable_auto_update.isEmpty()) {
        settings.setValue(kSparkleAlreadyEnableUpdateByDefault, true);
        setAutoUpdateEnabled(true);
    }
#else
    if (!already_enable_update_by_default) {
        settings.setValue(kSparkleAlreadyEnableUpdateByDefault, true);
        setAutoUpdateEnabled(true);
    }
#endif

    settings.endGroup();
}

void AutoUpdateService::stop()
{
    adapter_->stop();
}


void AutoUpdateService::checkUpdate()
{
    adapter_->checkNow();
}


bool AutoUpdateService::shouldSupportAutoUpdate() const {
    return QString(getBrand()) == "SeaDrive";
}

bool AutoUpdateService::autoUpdateEnabled() const {
    return adapter_->autoUpdateEnabled();
}

void AutoUpdateService::setAutoUpdateEnabled(bool enabled) {
    // qWarning() << "setAutoUpdateEnabled:" << enabled;
    adapter_->setAutoUpdateEnabled(enabled);
}
#endif // HAVE_SPARKLE_SUPPORT
