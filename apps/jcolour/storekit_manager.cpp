#include "storekit_manager.h"
#include <QSettings>
#include <QDebug>
#include <QDateTime>

namespace jcolour {
namespace store {

StoreKitManager& StoreKitManager::instance() {
    static StoreKitManager instance;
    return instance;
}

StoreKitManager::StoreKitManager(QObject* parent)
    : QObject(parent)
    , isInitialized_(false)
{
    loadCachedSubscription();
}

void StoreKitManager::initialize() {
    if (isInitialized_) return;

    // In production, would initialize StoreKit framework here
    // StoreKit::StoreKitManager::shared()

    isInitialized_ = true;
    qDebug() << "StoreKit initialized";

    // Load available products
    ProductInfo freeProduct;
    freeProduct.id = "com.jcolour.free";
    freeProduct.displayName = "jcolour Free";
    freeProduct.displayDescription = "Free tier with basic features";
    freeProduct.price = 0;
    freeProduct.priceString = "Free";
    freeProduct.currencyCode = "USD";
    freeProduct.tier = SubscriptionTier::Free;
    products_.append(freeProduct);

    ProductInfo starterProduct;
    starterProduct.id = "com.jcolour.starter";
    starterProduct.displayName = "jcolour Starter";
    starterProduct.displayDescription = "Starter subscription for casual users";
    starterProduct.price = 49.99;
    starterProduct.priceString = "$49.99/year";
    starterProduct.currencyCode = "USD";
    starterProduct.tier = SubscriptionTier::Starter;
    starterProduct.hasFreeTrial = true;
    starterProduct.freeTrialDays = 14;
    products_.append(starterProduct);

    ProductInfo proProduct;
    proProduct.id = "com.jcolour.pro";
    proProduct.displayName = "jcolour Pro";
    proProduct.displayDescription = "Full access to all features";
    proProduct.price = 99.99;
    proProduct.priceString = "$99.99/year";
    proProduct.currencyCode = "USD";
    proProduct.tier = SubscriptionTier::Pro;
    proProduct.hasFreeTrial = true;
    proProduct.freeTrialDays = 14;
    products_.append(proProduct);

    emit productsLoaded(products_);
}

QVector<ProductInfo> StoreKitManager::availableProducts() const {
    return products_;
}

bool StoreKitManager::requestProducts() {
    // In production, would request products from App Store
    // StoreKit::productsRequest with productIds

    if (products_.isEmpty()) {
        qWarning() << "No products loaded, call initialize first";
        return false;
    }

    emit productsLoaded(products_);
    return true;
}

ProductInfo StoreKitManager::getProduct(SubscriptionTier tier) const {
    for (const ProductInfo& product : products_) {
        if (product.tier == tier) {
            return product;
        }
    }
    return ProductInfo();
}

bool StoreKitManager::purchaseProduct(const QString& productId) {
    if (!canMakePayments()) {
        qWarning() << "Cannot make payments on this device";
        emit purchaseCompleted(false, "Cannot make payments");
        return false;
    }

    // In production, would initiate StoreKit purchase
    // StoreKit::PaymentQueue.default().addPayment(payment)

    // Simulate successful purchase
    for (const ProductInfo& product : products_) {
        if (product.id == productId) {
            currentSubscription_.tier = product.tier;
            currentSubscription_.status = SubscriptionStatus::Active;
            currentSubscription_.startDate = QDate::currentDate();
            currentSubscription_.expirationDate = QDate::currentDate().addDays(365);
            currentSubscription_.productId = productId;
            currentSubscription_.isAutoRenewing = true;

            entitlements_ = computeEntitlements(product.tier);
            saveSubscriptionToCache();

            emit purchaseCompleted(true, QString());
            emit subscriptionStatusChanged(SubscriptionStatus::Active);
            emit entitlementsChanged(entitlements_);

            return true;
        }
    }

    emit purchaseCompleted(false, "Product not found");
    return false;
}

bool StoreKitManager::restorePurchases() {
    // In production, would call StoreKit restore
    // StoreKit::PaymentQueue.default().restoreCompletedTransactions()

    // For now, just reload cached subscription
    loadCachedSubscription();

    if (currentSubscription_.status == SubscriptionStatus::Active) {
        emit subscriptionStatusChanged(currentSubscription_.status);
        emit entitlementsChanged(entitlements_);
        return true;
    }

    return false;
}

bool StoreKitManager::canMakePayments() const {
    // In production, would check StoreKit canMakePayments
    // SKPaymentQueue.canMakePayments()
    return true; // Simulated for testing
}

SubscriptionInfo StoreKitManager::getSubscriptionStatus() const {
    return currentSubscription_;
}

bool StoreKitManager::isEntitled(SubscriptionTier requiredTier) const {
    if (currentSubscription_.status != SubscriptionStatus::Active &&
        currentSubscription_.status != SubscriptionStatus::GracePeriod) {
        return false;
    }

    // Free tier is always entitled
    if (currentSubscription_.tier == SubscriptionTier::Free) {
        return requiredTier == SubscriptionTier::Free;
    }

    // Pro is entitled to everything
    if (currentSubscription_.tier == SubscriptionTier::Pro) {
        return true;
    }

    // Starter can use Free tier features
    if (currentSubscription_.tier == SubscriptionTier::Starter) {
        return requiredTier == SubscriptionTier::Free || requiredTier == SubscriptionTier::Starter;
    }

    return false;
}

Entitlements StoreKitManager::getEntitlements() const {
    return entitlements_;
}

bool StoreKitManager::isInGracePeriod() const {
    return currentSubscription_.status == SubscriptionStatus::GracePeriod;
}

QDate StoreKitManager::gracePeriodExpiration() const {
    return gracePeriodEndDate_;
}

QString StoreKitManager::latestReceipt() const {
    QSettings settings("jcolour", "storekit");
    return settings.value("receipt").toString();
}

bool StoreKitManager::validateReceipt() {
    // In production, would validate receipt with App Store
    // Apple receipt validation endpoint

    QString receipt = latestReceipt();
    if (receipt.isEmpty()) {
        return false;
    }

    // Simulated validation
    return true;
}

void StoreKitManager::setSubscriptionTier(SubscriptionTier tier) {
    currentSubscription_.tier = tier;
    currentSubscription_.status = SubscriptionStatus::Active;
    currentSubscription_.startDate = QDate::currentDate();
    currentSubscription_.expirationDate = QDate::currentDate().addYears(100); // Lifetime
    entitlements_ = computeEntitlements(tier);
    saveSubscriptionToCache();

    emit subscriptionStatusChanged(SubscriptionStatus::Active);
    emit entitlementsChanged(entitlements_);
}

void StoreKitManager::applyLifetimeUnlock(const QString& unlockCode) {
    // In production, would validate unlock code with backend
    // For now, just check if code starts with "UNLOCK-"

    if (unlockCode.startsWith("UNLOCK-")) {
        setSubscriptionTier(SubscriptionTier::Pro);
        qDebug() << "Lifetime unlock applied";
    }
}

void StoreKitManager::loadCachedSubscription() {
    QSettings settings("jcolour", "storekit");

    currentSubscription_.tier = static_cast<SubscriptionTier>(
        settings.value("tier", 0).toInt());
    currentSubscription_.status = static_cast<SubscriptionStatus>(
        settings.value("status", 0).toInt());
    currentSubscription_.startDate = settings.value("startDate").toDate();
    currentSubscription_.expirationDate = settings.value("expirationDate").toDate();
    currentSubscription_.isAutoRenewing = settings.value("autoRenew", false).toBool();
    currentSubscription_.productId = settings.value("productId").toString();

    entitlements_ = computeEntitlements(currentSubscription_.tier);

    // Check for grace period
    checkGracePeriod();
}

void StoreKitManager::saveSubscriptionToCache() {
    QSettings settings("jcolour", "storekit");

    settings.setValue("tier", static_cast<int>(currentSubscription_.tier));
    settings.setValue("status", static_cast<int>(currentSubscription_.status));
    settings.setValue("startDate", currentSubscription_.startDate);
    settings.setValue("expirationDate", currentSubscription_.expirationDate);
    settings.setValue("autoRenew", currentSubscription_.isAutoRenewing);
    settings.setValue("productId", currentSubscription_.productId);
}

Entitlements StoreKitManager::computeEntitlements(SubscriptionTier tier) {
    Entitlements ent;

    switch (tier) {
        case SubscriptionTier::Free:
            ent.canPrint = true;
            ent.canUseBWSingleBlack = true;
            ent.canUseBWMultiChannel = false;
            ent.canUsePerInkLinearization = false;
            ent.canUseAllRenderingIntents = false;
            ent.canUseSpotColor = false;
            ent.canUseTiling = false;
            ent.canUseNesting = false;
            ent.canUseContactSheet = false;
            ent.canUseG7Validation = false;
            ent.maxImagesInQueue = 5;
            ent.maxBatchSize = 10;
            break;

        case SubscriptionTier::Starter:
            ent.canPrint = true;
            ent.canUseBWSingleBlack = true;
            ent.canUseBWMultiChannel = true;
            ent.canUsePerInkLinearization = false;
            ent.canUseAllRenderingIntents = true;
            ent.canUseSpotColor = false;
            ent.canUseTiling = false;
            ent.canUseNesting = true;
            ent.canUseContactSheet = false;
            ent.canUseG7Validation = false;
            ent.maxImagesInQueue = 20;
            ent.maxBatchSize = 50;
            break;

        case SubscriptionTier::Pro:
            ent.canPrint = true;
            ent.canUseBWSingleBlack = true;
            ent.canUseBWMultiChannel = true;
            ent.canUsePerInkLinearization = true;
            ent.canUseAllRenderingIntents = true;
            ent.canUseSpotColor = true;
            ent.canUseTiling = true;
            ent.canUseNesting = true;
            ent.canUseContactSheet = true;
            ent.canUseG7Validation = true;
            ent.maxImagesInQueue = 100;
            ent.maxBatchSize = 500;
            break;
    }

    return ent;
}

bool StoreKitManager::checkGracePeriod() {
    if (currentSubscription_.status == SubscriptionStatus::Active &&
        currentSubscription_.expirationDate < QDate::currentDate()) {
        // Subscription expired, check grace period
        gracePeriodEndDate_ = currentSubscription_.expirationDate.addDays(7); // 7 day grace

        if (QDate::currentDate() <= gracePeriodEndDate_) {
            currentSubscription_.status = SubscriptionStatus::GracePeriod;
            return true;
        } else {
            currentSubscription_.status = SubscriptionStatus::Expired;
            return false;
        }
    }

    return currentSubscription_.status == SubscriptionStatus::GracePeriod;
}

} // namespace store
} // namespace jcolour