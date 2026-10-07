-- Выполняется приложением автоматически при запуске.
-- Для ручного применения к существующей SQLite БД.
PRAGMA foreign_keys = ON;
CREATE TABLE IF NOT EXISTS bank_category_rules (pattern TEXT NOT NULL CHECK(length(trim(pattern)) > 0), match_mode TEXT NOT NULL CHECK(match_mode IN ('exact','contains')), type INTEGER NOT NULL CHECK(type IN (0,1)), category_id TEXT NOT NULL REFERENCES categories(id) ON DELETE CASCADE, PRIMARY KEY(pattern,match_mode,type));
