# Money Tracker

Qt 6 / QML personal finance application for desktop and mobile.

The UI uses fully custom QML styling for the banking-style controls. No QSS is required.

## UI components

- `Desktop.qml` and `Mobile.qml` — platform-specific application shells.
- `BankComboBox.qml` — custom currency selector and popup.
- `CategoryPicker.qml` — custom category grid used instead of a native-looking dropdown.

Categories are loaded from SQLite. The Categories menu allows the user to add
text-only income and expense categories; duplicate names within one type are
rejected.

Existing categories can be renamed or removed. Removal is implemented as
archiving, so transactions that already reference the category keep their
category name while the category disappears from new-operation pickers.

## Asset hierarchy

The domain separates the fixed top-level asset sections from concrete user
accounts:

`Asset (Fiat / Crypto / Investment) -> Account -> Transaction`

An account represents a concrete source of value: cash, a debit or credit
card, a savings account, a crypto wallet, a broker, or a deposit. In SQLite,
`accounts.asset_type` assigns every account to its top-level asset section.

The selected asset is exposed by `FinanceController.selectedAsset` and saved in
the settings table. The Accounts menu lists only accounts belonging to that
asset and creates new accounts with the selected asset type.

The default transaction currency and application currency are RUB.

## TRON USDT wallets

The Crypto asset can track a public TRON Mainnet address and its USDT TRC-20
balance. Public addresses are validated locally with Base58Check before they
are stored. Private keys and seed phrases are never requested or stored.

Balances are read directly from the official USDT contract through TronGrid's
read-only `triggerconstantcontract` endpoint. The USDT/USD quote is read from
CoinGecko. Successful snapshots are stored in SQLite; a failed request keeps
the previous balance and price. Refreshes run every six hours while the app is
open, and once on startup only when the saved data is stale. The network code
is isolated in `TronUsdtProvider`, so these requests can later be routed
through an application server without changing the database or QML UI.

## Storage

Transactions, accounts, categories and application settings are stored in the
SQLite database provided by Qt SQL. The database is created automatically in
the platform-specific application data directory.

On startup, account balances and income/expense totals are aggregated by
currency in SQLite and kept in memory. New transactions are committed to the
database synchronously before the in-memory state and QML interface are
updated.

## Currency rates

The application requests the official daily RUB exchange rates from the Bank
of Russia over HTTPS. A refresh is attempted at most once every 12 hours while
the application is running, and immediately on startup when the last successful
result is older than 12 hours.

The response must contain valid USD and EUR quotes. Only then is the local JSON
cache replaced using an atomic file commit. Network errors, non-200 responses,
malformed XML, missing quotes, and older rate dates leave the previous cache
untouched. Until the first successful request, the bundled fallback rates are
used.

Automatic updates can be disabled in Settings. In manual mode the application
makes no rate requests and uses the saved `USD/RUB` and `EUR/RUB` values instead.
Manual values are stored independently from the last successful automatic
cache, so switching modes never overwrites either set. A future version can
extend the same settings section with a choice of automatic rate provider.

## CSV import and export

Settings provides import and export for all transactions. Ledgera writes
UTF-8 CSV with a BOM and semicolon separators so the file opens correctly in
spreadsheet applications configured for Russian locales. Amounts are stored as
integer minor units, which avoids rounding money during a round trip.

Income and expense transactions occupy one row each. A transfer also occupies
one row and contains both source and target amounts, accounts, and currencies;
it is restored as the application's atomic outgoing/incoming pair. Import
validates every row before changing SQLite, inserts the complete file in one
database transaction, and skips operation identifiers that already exist.
