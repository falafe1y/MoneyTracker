#include "CryptoParser.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace
{

constexpr char kBase58Alphabet[] =
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
constexpr char kBech32Alphabet[] =
    "qpzry9x8gf2tvdw0s3jn54khce6mua7l";
constexpr int kEthereumSourceDecimals = 18;
constexpr int kEthereumStoredDecimals = 8;

void setError(QString* error, QString message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

int alphabetValue(const QChar character, const char* alphabet)
{
    const ushort unicode = character.unicode();
    if (unicode > 0x7f) {
        return -1;
    }
    const char* position = std::strchr(
        alphabet, static_cast<char>(unicode));
    return position == nullptr
        ? -1
        : static_cast<int>(position - alphabet);
}

bool isHex(const QString& value, const qsizetype expectedSize)
{
    if (value.size() != expectedSize) {
        return false;
    }
    return std::all_of(
        value.cbegin(),
        value.cend(),
        [](const QChar character)
        {
            const ushort code = character.unicode();
            return (code >= '0' && code <= '9') ||
                   (code >= 'a' && code <= 'f') ||
                   (code >= 'A' && code <= 'F');
        });
}

bool decodeBitcoinBase58(const QString& address, QByteArray& decoded)
{
    if (address.size() < 26 || address.size() > 35 ||
        (address.front() != QLatin1Char('1') &&
         address.front() != QLatin1Char('3'))) {
        return false;
    }

    QByteArray bytes(25, '\0');
    for (const QChar character : address) {
        const int digit = alphabetValue(character, kBase58Alphabet);
        if (digit < 0) {
            return false;
        }
        int carry = digit;
        for (qsizetype index = bytes.size() - 1; index >= 0; --index) {
            carry += static_cast<unsigned char>(bytes[index]) * 58;
            bytes[index] = static_cast<char>(carry & 0xff);
            carry >>= 8;
        }
        if (carry != 0) {
            return false;
        }
    }

    const unsigned char prefix = static_cast<unsigned char>(bytes.front());
    if (prefix != 0x00 && prefix != 0x05) {
        return false;
    }
    qsizetype leadingOnes = 0;
    while (leadingOnes < address.size() &&
           address[leadingOnes] == QLatin1Char('1')) {
        ++leadingOnes;
    }
    qsizetype leadingZeroBytes = 0;
    while (leadingZeroBytes < bytes.size() &&
           bytes[leadingZeroBytes] == '\0') {
        ++leadingZeroBytes;
    }
    if (leadingOnes != leadingZeroBytes) {
        return false;
    }
    const QByteArray body = bytes.first(21);
    const QByteArray firstHash = QCryptographicHash::hash(
        body, QCryptographicHash::Sha256);
    const QByteArray checksum = QCryptographicHash::hash(
        firstHash, QCryptographicHash::Sha256).first(4);
    if (checksum != bytes.last(4)) {
        return false;
    }
    decoded = bytes;
    return true;
}

quint32 bech32Polymod(const QVector<int>& values)
{
    static constexpr std::array<quint32, 5> generators{
        0x3b6a57b2U, 0x26508e6dU, 0x1ea119faU,
        0x3d4233ddU, 0x2a1462b3U};
    quint32 checksum = 1;
    for (const int value : values) {
        const quint32 high = checksum >> 25;
        checksum = ((checksum & 0x1ffffffU) << 5) ^
            static_cast<quint32>(value);
        for (int bit = 0; bit < 5; ++bit) {
            if (((high >> bit) & 1U) != 0U) {
                checksum ^= generators[static_cast<std::size_t>(bit)];
            }
        }
    }
    return checksum;
}

bool convertWitnessProgram(
    const QVector<int>& values,
    QByteArray& program
    )
{
    quint32 accumulator = 0;
    int bits = 0;
    program.clear();
    for (const int value : values) {
        if (value < 0 || value > 31) {
            return false;
        }
        accumulator = (accumulator << 5) | static_cast<quint32>(value);
        bits += 5;
        while (bits >= 8) {
            bits -= 8;
            program.append(static_cast<char>((accumulator >> bits) & 0xffU));
        }
    }
    return bits < 5 && ((accumulator << (8 - bits)) & 0xffU) == 0U;
}

bool validateBitcoinBech32(const QString& address)
{
    if (address.size() < 14 || address.size() > 90) {
        return false;
    }
    const bool hasLower = address != address.toUpper();
    const bool hasUpper = address != address.toLower();
    if (hasLower && hasUpper) {
        return false;
    }

    const QString normalized = address.toLower();
    if (!normalized.startsWith(QStringLiteral("bc1"))) {
        return false;
    }
    const qsizetype separator = normalized.lastIndexOf(QLatin1Char('1'));
    if (separator != 2 || normalized.size() - separator - 1 < 7) {
        return false;
    }

    QVector<int> values;
    values.reserve(normalized.size() + 5);
    const QString humanReadable = normalized.left(separator);
    for (const QChar character : humanReadable) {
        values.append(character.unicode() >> 5);
    }
    values.append(0);
    for (const QChar character : humanReadable) {
        values.append(character.unicode() & 31);
    }

    QVector<int> data;
    data.reserve(normalized.size() - separator - 1);
    for (qsizetype index = separator + 1; index < normalized.size(); ++index) {
        const int value = alphabetValue(normalized[index], kBech32Alphabet);
        if (value < 0) {
            return false;
        }
        values.append(value);
        data.append(value);
    }

    const quint32 checksum = bech32Polymod(values);
    const int witnessVersion = data.first();
    if (witnessVersion < 0 || witnessVersion > 16 ||
        (witnessVersion == 0 && checksum != 1U) ||
        (witnessVersion != 0 && checksum != 0x2bc830a3U)) {
        return false;
    }

    data.removeFirst();
    data.resize(data.size() - 6);
    QByteArray program;
    if (!convertWitnessProgram(data, program) ||
        program.size() < 2 || program.size() > 40) {
        return false;
    }
    return witnessVersion != 0 || program.size() == 20 || program.size() == 32;
}

bool jsonInteger(const QJsonValue& value, qint64& result)
{
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || number < 0.0 ||
        number > static_cast<double>(std::numeric_limits<qint64>::max()) ||
        std::floor(number) != number) {
        return false;
    }
    result = static_cast<qint64>(number);
    return true;
}

bool parseScaledUnsignedDecimal(
    QString text,
    const int sourceDecimals,
    const int targetDecimals,
    qint64& value
    )
{
    text = text.trimmed();
    if (text.isEmpty() || sourceDecimals < 0 || targetDecimals < 0 ||
        !std::all_of(
            text.cbegin(), text.cend(),
            [](const QChar character) { return character.isDigit(); })) {
        return false;
    }
    while (text.size() > 1 && text.startsWith(QLatin1Char('0'))) {
        text.removeFirst();
    }

    if (sourceDecimals > targetDecimals) {
        const int discarded = sourceDecimals - targetDecimals;
        text = text.size() <= discarded
            ? QStringLiteral("0")
            : text.left(text.size() - discarded);
    } else if (sourceDecimals < targetDecimals) {
        text.append(QString(
            targetDecimals - sourceDecimals,
            QLatin1Char('0')));
    }
    bool ok = false;
    value = text.toLongLong(&ok);
    return ok && value >= 0;
}

QString bitcoinInputAddress(const QJsonObject& input)
{
    return input.value(QStringLiteral("prevout")).toObject()
        .value(QStringLiteral("scriptpubkey_address")).toString().trimmed();
}

QString bitcoinOutputAddress(const QJsonObject& output)
{
    return output.value(QStringLiteral("scriptpubkey_address"))
        .toString().trimmed();
}

} // namespace

bool isValidBitcoinAddress(const QString& address)
{
    const QString normalized = address.trimmed();
    QByteArray decoded;
    return decodeBitcoinBase58(normalized, decoded) ||
           validateBitcoinBech32(normalized);
}

QString normalizeBitcoinAddress(const QString& address)
{
    const QString trimmed = address.trimmed();
    if (!isValidBitcoinAddress(trimmed)) {
        return {};
    }
    return trimmed.startsWith(QStringLiteral("bc1"), Qt::CaseInsensitive)
        ? trimmed.toLower()
        : trimmed;
}

bool isValidEthereumAddress(const QString& address)
{
    const QString normalized = address.trimmed();
    return normalized.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive) &&
           isHex(normalized.sliced(2), 40);
}

QString normalizeEthereumAddress(const QString& address)
{
    return isValidEthereumAddress(address)
        ? address.trimmed().toLower()
        : QString();
}

bool parseBitcoinBalanceResponse(
    const QByteArray& response,
    qint64& balanceSatoshis,
    QString* error
    )
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("Invalid JSON returned by mempool.space"));
        return false;
    }
    const QJsonObject stats = document.object()
        .value(QStringLiteral("chain_stats")).toObject();
    qint64 funded = 0;
    qint64 spent = 0;
    if (!jsonInteger(stats.value(QStringLiteral("funded_txo_sum")), funded) ||
        !jsonInteger(stats.value(QStringLiteral("spent_txo_sum")), spent) ||
        spent > funded) {
        setError(error, QStringLiteral("mempool.space returned an invalid BTC balance"));
        return false;
    }
    balanceSatoshis = funded - spent;
    return true;
}

bool parseBitcoinTransactionsResponse(
    const QByteArray& response,
    const QString& walletId,
    const QString& address,
    QVector<CryptoTransaction>& transactions,
    QString* error
    )
{
    transactions.clear();
    const QString normalizedAddress = normalizeBitcoinAddress(address);
    if (walletId.trimmed().isEmpty() || normalizedAddress.isEmpty()) {
        setError(error, QStringLiteral("Invalid BTC wallet identity"));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        setError(error, QStringLiteral("Invalid transaction JSON returned by mempool.space"));
        return false;
    }

    for (const QJsonValue& value : document.array()) {
        if (!value.isObject()) {
            setError(error, QStringLiteral("mempool.space returned an invalid transaction"));
            transactions.clear();
            return false;
        }
        const QJsonObject transaction = value.toObject();
        const QJsonObject status = transaction
            .value(QStringLiteral("status")).toObject();
        if (!status.value(QStringLiteral("confirmed")).toBool(false)) {
            continue;
        }
        const QString transactionId = transaction
            .value(QStringLiteral("txid")).toString().trimmed();
        qint64 timestamp = 0;
        if (!isHex(transactionId, 64) ||
            !jsonInteger(status.value(QStringLiteral("block_time")), timestamp) ||
            timestamp <= 0) {
            setError(error, QStringLiteral("mempool.space returned invalid BTC transaction fields"));
            transactions.clear();
            return false;
        }

        qint64 sentByWallet = 0;
        qint64 receivedByWallet = 0;
        QString incomingCounterparty;
        QString outgoingCounterparty;
        for (const QJsonValue& inputValue : transaction
                 .value(QStringLiteral("vin")).toArray()) {
            const QJsonObject input = inputValue.toObject();
            const QJsonObject previousOutput = input
                .value(QStringLiteral("prevout")).toObject();
            const QString inputAddress = bitcoinInputAddress(input);
            qint64 amount = 0;
            if (!previousOutput.isEmpty() &&
                !jsonInteger(previousOutput.value(QStringLiteral("value")), amount)) {
                setError(error, QStringLiteral("mempool.space returned an invalid BTC input"));
                transactions.clear();
                return false;
            }
            if (normalizeBitcoinAddress(inputAddress) == normalizedAddress) {
                if (amount > std::numeric_limits<qint64>::max() - sentByWallet) {
                    setError(error, QStringLiteral("BTC transaction amount is too large"));
                    transactions.clear();
                    return false;
                }
                sentByWallet += amount;
            } else if (incomingCounterparty.isEmpty() && !inputAddress.isEmpty()) {
                incomingCounterparty = inputAddress;
            }
        }
        for (const QJsonValue& outputValue : transaction
                 .value(QStringLiteral("vout")).toArray()) {
            const QJsonObject output = outputValue.toObject();
            const QString outputAddress = bitcoinOutputAddress(output);
            qint64 amount = 0;
            if (!jsonInteger(output.value(QStringLiteral("value")), amount)) {
                setError(error, QStringLiteral("mempool.space returned an invalid BTC output"));
                transactions.clear();
                return false;
            }
            if (normalizeBitcoinAddress(outputAddress) == normalizedAddress) {
                if (amount > std::numeric_limits<qint64>::max() - receivedByWallet) {
                    setError(error, QStringLiteral("BTC transaction amount is too large"));
                    transactions.clear();
                    return false;
                }
                receivedByWallet += amount;
            } else if (outgoingCounterparty.isEmpty() && !outputAddress.isEmpty()) {
                outgoingCounterparty = outputAddress;
            }
        }

        const qint64 net = receivedByWallet - sentByWallet;
        if (net == 0) {
            continue;
        }
        const bool outgoing = net < 0;
        const QString counterparty = outgoing
            ? (outgoingCounterparty.isEmpty()
                   ? QStringLiteral("multiple recipients")
                   : outgoingCounterparty)
            : (incomingCounterparty.isEmpty()
                   ? QStringLiteral("coinbase")
                   : incomingCounterparty);
        transactions.append(CryptoTransaction(
            walletId,
            transactionId,
            outgoing ? normalizedAddress : counterparty,
            outgoing ? counterparty : normalizedAddress,
            outgoing ? -net : net,
            QDateTime::fromSecsSinceEpoch(timestamp, Qt::UTC)));
    }
    return true;
}

bool parseEthereumBalanceResponse(
    const QByteArray& response,
    qint64& balanceAtomic,
    QString* error
    )
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("Invalid JSON returned by Blockscout"));
        return false;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("status")).toString() != QStringLiteral("1") ||
        !parseScaledUnsignedDecimal(
            root.value(QStringLiteral("result")).toString(),
            kEthereumSourceDecimals,
            kEthereumStoredDecimals,
            balanceAtomic)) {
        setError(error, QStringLiteral("Blockscout returned an invalid ETH balance"));
        return false;
    }
    return true;
}

bool parseEthereumTransactionsResponse(
    const QByteArray& response,
    const QString& walletId,
    const QString& address,
    QVector<CryptoTransaction>& transactions,
    QString* error
    )
{
    transactions.clear();
    const QString normalizedAddress = normalizeEthereumAddress(address);
    if (walletId.trimmed().isEmpty() || normalizedAddress.isEmpty()) {
        setError(error, QStringLiteral("Invalid ETH wallet identity"));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("Invalid transaction JSON returned by Blockscout"));
        return false;
    }
    const QJsonObject root = document.object();
    const QJsonValue result = root.value(QStringLiteral("result"));
    const QString status = root.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("0") &&
        ((result.isArray() && result.toArray().isEmpty()) ||
         result.toString().compare(
             QStringLiteral("No transactions found"),
             Qt::CaseInsensitive) == 0)) {
        return true;
    }
    if (status != QStringLiteral("1") || !result.isArray()) {
        setError(error, QStringLiteral("Blockscout rejected the ETH transaction request"));
        return false;
    }

    for (const QJsonValue& value : result.toArray()) {
        if (!value.isObject()) {
            setError(error, QStringLiteral("Blockscout returned an invalid ETH transaction"));
            transactions.clear();
            return false;
        }
        const QJsonObject item = value.toObject();
        if (item.value(QStringLiteral("isError")).toString() == QStringLiteral("1") ||
            item.value(QStringLiteral("txreceipt_status")).toString() == QStringLiteral("0")) {
            continue;
        }
        const QString transactionId = item.value(
            QStringLiteral("hash")).toString().trimmed().toLower();
        const QString from = normalizeEthereumAddress(item.value(
            QStringLiteral("from")).toString());
        QString to = normalizeEthereumAddress(item.value(
            QStringLiteral("to")).toString());
        qint64 amountAtomic = 0;
        bool timestampOk = false;
        const qint64 timestamp = item.value(QStringLiteral("timeStamp"))
            .toString().toLongLong(&timestampOk);
        bool confirmationsOk = false;
        const qint64 confirmations = item.value(QStringLiteral("confirmations"))
            .toString().toLongLong(&confirmationsOk);
        if (to.isEmpty() && from == normalizedAddress &&
            item.value(QStringLiteral("to")).toString().trimmed().isEmpty()) {
            to = QStringLiteral("contract creation");
        }
        if (!transactionId.startsWith(QStringLiteral("0x")) ||
            !isHex(transactionId.mid(2), 64) || from.isEmpty() || to.isEmpty() ||
            !parseScaledUnsignedDecimal(
                item.value(QStringLiteral("value")).toString(),
                kEthereumSourceDecimals,
                kEthereumStoredDecimals,
                amountAtomic) ||
            !timestampOk || timestamp <= 0 || !confirmationsOk || confirmations <= 0) {
            setError(error, QStringLiteral("Blockscout returned invalid ETH transaction fields"));
            transactions.clear();
            return false;
        }
        if (amountAtomic == 0 ||
            (from != normalizedAddress && to != normalizedAddress)) {
            continue;
        }
        transactions.append(CryptoTransaction(
            walletId,
            transactionId,
            from,
            to,
            amountAtomic,
            QDateTime::fromSecsSinceEpoch(timestamp, Qt::UTC)));
    }
    return true;
}

bool parseCoinGeckoPricesResponse(
    const QByteArray& response,
    QHash<QString, qint64>& pricesUsdMicros,
    QString* error
    )
{
    pricesUsdMicros.clear();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("Invalid JSON returned by CoinGecko"));
        return false;
    }

    static const std::array<std::pair<QString, QString>, 3> mappings{{
        {QStringLiteral("tether"), QStringLiteral("USDT")},
        {QStringLiteral("bitcoin"), QStringLiteral("BTC")},
        {QStringLiteral("ethereum"), QStringLiteral("ETH")}}};
    const QJsonObject root = document.object();
    for (const auto& [coinGeckoId, symbol] : mappings) {
        const double price = root.value(coinGeckoId).toObject()
            .value(QStringLiteral("usd")).toDouble(0.0);
        const double scaled = price * 1'000'000.0;
        if (!std::isfinite(price) || price <= 0.0 ||
            !std::isfinite(scaled) ||
            scaled > static_cast<double>(std::numeric_limits<qint64>::max())) {
            setError(error, QStringLiteral("CoinGecko returned an invalid %1 price")
                                .arg(symbol));
            pricesUsdMicros.clear();
            return false;
        }
        pricesUsdMicros.insert(
            symbol, static_cast<qint64>(std::llround(scaled)));
    }
    return true;
}
