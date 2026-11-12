PRAGMA user_version = 1;

CREATE TABLE IF NOT EXISTS knowledge (
  id            INTEGER PRIMARY KEY,
  kind          TEXT NOT NULL CHECK (kind IN
                  ('fact','preference','decision','temporary','unverified','historical')),
  category      TEXT NOT NULL,
  key           TEXT NOT NULL,
  value         TEXT NOT NULL,
  source        TEXT NOT NULL,
  confidence    REAL NOT NULL DEFAULT 1.0,
  status        TEXT NOT NULL CHECK (status IN ('confirmed','unverified','superseded')),
  created_at    TEXT NOT NULL,
  updated_at    TEXT NOT NULL,
  expires_at    TEXT,
  hash          TEXT NOT NULL,
  superseded_by INTEGER
);
CREATE INDEX IF NOT EXISTS idx_knowledge_hash ON knowledge(hash);
CREATE INDEX IF NOT EXISTS idx_knowledge_ck ON knowledge(category, key);

CREATE VIRTUAL TABLE IF NOT EXISTS knowledge_fts USING fts5(
  key, value, category,
  content='knowledge', content_rowid='id', tokenize='porter unicode61'
);

CREATE TRIGGER IF NOT EXISTS knowledge_ai AFTER INSERT ON knowledge BEGIN
  INSERT INTO knowledge_fts(rowid, key, value, category) VALUES (new.id, new.key, new.value, new.category);
END;
CREATE TRIGGER IF NOT EXISTS knowledge_ad AFTER DELETE ON knowledge BEGIN
  INSERT INTO knowledge_fts(knowledge_fts, rowid, key, value, category) VALUES ('delete', old.id, old.key, old.value, old.category);
END;
CREATE TRIGGER IF NOT EXISTS knowledge_au AFTER UPDATE OF key, value, category ON knowledge BEGIN
  INSERT INTO knowledge_fts(knowledge_fts, rowid, key, value, category) VALUES ('delete', old.id, old.key, old.value, old.category);
  INSERT INTO knowledge_fts(rowid, key, value, category) VALUES (new.id, new.key, new.value, new.category);
END;
