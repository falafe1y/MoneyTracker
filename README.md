# Money Tracker

Qt 6 / QML personal finance desktop application.

The UI uses fully custom QML styling for the banking-style controls. No QSS is required.

## UI components

- `Main.qml` — main application shell, balance card, transaction list and add-transaction dialog.
- `BankComboBox.qml` — custom currency selector and popup.
- `CategoryPicker.qml` — custom category grid used instead of a native-looking dropdown.

Categories are loaded from SQLite. The Categories menu allows the user to add
text-only income and expense categories; duplicate names within one type are
rejected.

Existing categories can be renamed or removed. Removal is implemented as
archiving, so transactions that already reference the category keep their
category name while the category disappears from new-operation pickers.

The default transaction currency and application currency are RUB.

## Storage

Transactions, accounts, categories and application settings are stored in the
SQLite database provided by Qt SQL. The database is created automatically in
the platform-specific application data directory.

On startup, account balances and income/expense totals are aggregated by
currency in SQLite and kept in memory. New transactions are committed to the
database synchronously before the in-memory state and QML interface are
updated.
