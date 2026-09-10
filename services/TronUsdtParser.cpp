#include "TronUsdtParser.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace
{

constexpr char kBase58Alphabet[] =
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
const QString kUsdtContract =
    QStringLiteral("TR7NHqjeKQxGTCi8q8ZY4pL8otSzgjLj6t");

void setError(QString* error, QString message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

int base58Value(const QChar character)
{
    const ushort unicode = character.unicode();
    if (unicode > 0x7f) {
        return -1;
    }
    const char* position = std::strchr(
        kBase58Alphabet, static_cast<char>(unicode));
    return position == nullptr
        ? -1
        : static_cast<int>(position - kBase58Alphabet);
}

bool parseUnsignedHex64(const QString& text, qint64& value)
{
    QString normalized = text.trimmed();
    if (normalized.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        normalized.remove(0, 2);
    }
    while (normalized.startsWith(QLatin1Char('0')) && normalized.size() > 1) {
        normalized.remove(0, 1);
    }
    if (normalized.isEmpty()) {
        value = 0;
        return true;
    }
    if (normalized.size() > 16) {
        return false;
    }

    bool ok = false;
    const qulonglong parsed = normalized.toULongLong(&ok, 16);
    if (!ok || parsed > static_cast<qulonglong>(
            std::numeric_limits<qint64>::max())) {
        return false;
    }
    value = static_cast<qint64>(parsed);
    return true;
}

} // namespace

bool decodeTronAddress(
    const QString& address,
    QByteArray& payload,
    QString* error
    )
{
    const QString normalized = address.trimmed();
    if (normalized.size() != 34 || !normalized.startsWith(QLatin1Char('T'))) {
        setError(error, QStringLiteral("TRON address must contain 34 characters and start with T"));
        return false;
    }

    QByteArray decoded(25, '\0');
    for (const QChar character : normalized) {
        const int digit = base58Value(character);
        if (digit < 0) {
            setError(error, QStringLiteral("TRON address contains a non-Base58 character"));
            return false;
        }

        int carry = digit;
        for (qsizetype index = decoded.size() - 1; index >= 0; --index) {
            carry += static_cast<unsigned char>(decoded[index]) * 58;
            decoded[index] = static_cast<char>(carry & 0xff);
            carry >>= 8;
        }
        if (carry != 0) {
            setError(error, QStringLiteral("TRON address is too large"));
            return false;
        }
    }

    if (static_cast<unsigned char>(decoded.front()) != 0x41) {
        setError(error, QStringLiteral("Address does not belong to TRON Mainnet"));
        return false;
    }

    const QByteArray body = decoded.first(21);
    const QByteArray firstHash = QCryptographicHash::hash(
        body, QCryptographicHash::Sha256);
    const QByteArray checksum = QCryptographicHash::hash(
        firstHash, QCryptographicHash::Sha256).first(4);
    if (checksum != decoded.last(4)) {
        setError(error, QStringLiteral("Invalid TRON address checksum"));
        return false;
    }

    payload = body;
    return true;
}

bool isValidTronAddress(const QString& address)
{
    QByteArray payload;
    return decodeTronAddress(address, payload);
}

QString tronUsdtBalanceParameter(const QString& address)
{
    QByteArray payload;
    if (!decodeTronAddress(address, payload)) {
        return {};
    }

    // ABI address values contain the last 20 bytes of the TRON address,
    // left-padded to a 32-byte word. The 0x41 network prefix is omitted.
    return QStringLiteral("000000000000000000000000")
        + QString::fromLatin1(payload.sliced(1).toHex());
}

bool parseTronUsdtBalanceResponse(
    const QByteArray& response,
    qint64& balanceAtomic,
    QString* error
    )
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("Invalid JSON returned by TRON"));
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonObject result = root.value(QStringLiteral("result")).toObject();
    if (!result.value(QStringLiteral("result")).toBool(false)) {
        setError(error, QStringLiteral("TRON rejected the balance request"));
        return false;
    }

    const QJsonArray values = root.value(QStringLiteral("constant_result")).toArray();
    if (values.isEmpty() || !values.first().isString() ||
        !parseUnsignedHex64(values.first().toString(), balanceAtomic)) {
        setError(error, QStringLiteral("TRON returned an invalid USDT balance"));
        return false;
    }
    return true;
}

bool parseCoinGeckoUsdtPriceResponse(
    const QByteArray& response,
    qint64& priceUsdMicros,
    QString* error
    )
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("Invalid JSON returned by CoinGecko"));
        return false;
    }

    const double price = document.object()
        .value(QStringLiteral("tether")).toObject()
        .value(QStringLiteral("usd")).toDouble(0.0);
    const double scaled = price * 1'000'000.0;
    if (!std::isfinite(price) || price <= 0.0 ||
        !std::isfinite(scaled) ||
        scaled > static_cast<double>(std::numeric_limits<qint64>::max())) {
        setError(error, QStringLiteral("CoinGecko returned an invalid USDT price"));
        return false;
    }

    priceUsdMicros = static_cast<qint64>(std::llround(scaled));
    return priceUsdMicros > 0;
}

bool parseTronUsdtTransactionsResponse(
    const QByteArray& response,
    const QString& walletId,
    QVector<CryptoTransaction>& transactions,
    QString* error
    )
{
    transactions.clear();
    if (walletId.trimmed().isEmpty()) {
        setError(error, QStringLiteral("Crypto wallet id is empty"));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("Invalid transaction JSON returned by TRON"));
        return false;
    }

    const QJsonObject root = document.object();
    if (!root.value(QStringLiteral("success")).toBool(false) ||
        !root.value(QStringLiteral("data")).isArray()) {
        setError(error, QStringLiteral("TRON rejected the transaction request"));
        return false;
    }

    const QJsonArray data = root.value(QStringLiteral("data")).toArray();
    transactions.reserve(data.size());
    for (const QJsonValue& value : data) {
        if (!value.isObject()) {
            setError(error, QStringLiteral("TRON returned an invalid transaction"));
            transactions.clear();
            return false;
        }

        const QJsonObject item = value.toObject();
        const QString transactionId = item.value(
            QStringLiteral("transaction_id")).toString().trimmed();
        const QString fromAddress = item.value(
            QStringLiteral("from")).toString().trimmed();
        const QString toAddress = item.value(
            QStringLiteral("to")).toString().trimmed();
        const QString amountText = item.value(
            QStringLiteral("value")).toString().trimmed();
        const QJsonObject token = item.value(
            QStringLiteral("token_info")).toObject();

        bool amountOk = false;
        const qint64 amountAtomic = amountText.toLongLong(&amountOk);
        const qint64 timestamp = static_cast<qint64>(item.value(
            QStringLiteral("block_timestamp")).toDouble(0.0));
        const QDateTime occurredAtUtc = QDateTime::fromMSecsSinceEpoch(
            timestamp, Qt::UTC);

        if (transactionId.isEmpty() || !isValidTronAddress(fromAddress) ||
            !isValidTronAddress(toAddress) || !amountOk || amountAtomic <= 0 ||
            timestamp <= 0 || !occurredAtUtc.isValid() ||
            token.value(QStringLiteral("address")).toString() != kUsdtContract ||
            token.value(QStringLiteral("decimals")).toInt(-1) != 6) {
            setError(error, QStringLiteral("TRON returned invalid USDT transaction fields"));
            transactions.clear();
            return false;
        }

        transactions.append(CryptoTransaction(
            walletId,
            transactionId,
            fromAddress,
            toAddress,
            amountAtomic,
            occurredAtUtc));
    }
    return true;
}
