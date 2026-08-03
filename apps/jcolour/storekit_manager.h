#pragma once

#include <QObject>
#include <QString>
#include <QDate>
#include <QVariantMap>

namespace jcolour {
namespace store {

enum class SubscriptionTier {
    Free,
    Starter,
    Pro
};

enum class SubscriptionStatus {
    Unknown,
    Active,
    Expired,
    Cancelled,
    Pending,
    GracePeriod
};

struct SubscriptionInfo {
    SubscriptionTier tier = SubscriptionTier::Free;
    SubscriptionStatus status = SubscriptionStatus::Unknown;
    QDate startDate;
    QDate expirationDate;
    bool isAutoRenewing = false;
    QString productId;
    QString errorMessage;
};

struct ProductInfo {
    QString id;
    QString displayName;
    QString displayDescription;
    double price; // In user's local currency
    QString priceString; // Formatted string like "$99.99/year"
    QString currencyCode;
    SubscriptionTier tier;

    // For subscription products
    int subscriptionDurationDays = 365;
    bool hasFreeTrial = false;
    int freeTrialDays = 0;
};

struct Entitlements {
    bool canPrint = false;
    bool canUseBWSingleBlack = false;
    bool canUseBWMultiChannel = false;
    bool canUsePerInkLinearization = false;
    bool canUseAllRenderingIntents = false;
    bool canUseSpotColor = false;
    bool canUseTiling = false;
    bool canUseNesting = false;
    bool canUseContactSheet = false;
    bool canUseG7Validation = false;
    int maxImagesInQueue = 5;
    int maxBatchSize = 10;
};

class StoreKitManager : public QObject {
    Q_OBJECT
public:
    static StoreKitManager& instance();

    // Initialize StoreKit (call on app startup)
    void initialize();

    // Products
    QVector<ProductInfo> availableProducts() const;
    bool requestProducts();
    ProductInfo getProduct(SubscriptionTier tier) const;

    // Purchases
    bool purchaseProduct(const QString& productId);
    bool restorePurchases();
    bool canMakePayments() const;

    // Subscription management
    SubscriptionInfo getSubscriptionStatus() const;
    bool isEntitled(SubscriptionTier requiredTier) const;
    Entitlements getEntitlements() const;

    // Grace period handling
    bool isInGracePeriod() const;
    QDate gracePeriodExpiration() const;

    // Receipt validation
    QString latestReceipt() const;
    bool validateReceipt();

    // User preferences
    void setSubscriptionTier(SubscriptionTier tier);
    void applyLifetimeUnlock(const QString& unlockCode);

signals:
    void productsLoaded(const QVector<ProductInfo>& products);
    void purchaseCompleted(bool success, const QString& error);
    void subscriptionStatusChanged(SubscriptionStatus status);
    void entitlementsChanged(const Entitlements& entitlements);
    void freeTrialExpiring(int daysRemaining);

private:
    StoreKitManager(QObject* parent = nullptr);
    ~StoreKitManager() = default;

    void loadCachedSubscription();
    void saveSubscriptionToCache();
    Entitlements computeEntitlements(SubscriptionTier tier);
    bool checkGracePeriod();

    SubscriptionInfo currentSubscription_;
    QVector<ProductInfo> products_;
    Entitlements entitlements_;
    bool isInitialized_ = false;
    QDate gracePeriodEndDate_;
};

} // namespace store
} // namespace jcolour