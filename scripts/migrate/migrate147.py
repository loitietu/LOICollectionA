import os
import sqlite3
from typing import List, Optional
 
class SQLiteStorage:
    def __init__(self, db_path: str):
        self.conn = sqlite3.connect(db_path)
        self.conn.execute("PRAGMA foreign_keys = 1;")

    def __del__(self):
        self.conn.close()

    def create(self, table: str) -> None:
        self.conn.execute(f"CREATE TABLE IF NOT EXISTS {table} (key TEXT PRIMARY KEY, value TEXT);")
        self.conn.commit()

    def insert(self, table: str, key: str, value: str) -> None:
        if self.has(table, key):
            self.update(table, key, value)
        else:
            self.conn.execute(f"INSERT OR REPLACE INTO {table} VALUES (?, ?);", (key, value))
            self.conn.commit()

    def update(self, table: str, key: str, value: str) -> None:
        self.conn.execute(f"UPDATE {table} SET value = ? WHERE key = ?;", (value, key))
        self.conn.commit()

    def has(self, table: str, key: Optional[str] = None) -> bool:
        if key is not None:
            cursor = self.conn.execute(f"SELECT 1 FROM {table} WHERE key = ?;", (key,))
        else:
            cursor = self.conn.execute("SELECT name FROM sqlite_master WHERE type='table' AND name = ?;", (table,))
        return cursor.fetchone() is not None

    def get(self, table: str, key: str, default_val: str = "") -> str:
        if not self.has(table) or not self.has(table, key):
            return default_val
        
        cursor = self.conn.execute(f"SELECT value FROM {table} WHERE key = ?;", (key,))
        result = cursor.fetchone()
        return result[0] if result else default_val

    def tables(self) -> List[str]:
        cursor = self.conn.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;")
        return [row[0] for row in cursor.fetchall()]
    
if __name__ == "__main__":
    mDataMigrateMap = {}

    LanguageDB = SQLiteStorage("./data/language.db")
    TpaDB = SQLiteStorage("./data/tpa.db")
    PvpDB = SQLiteStorage("./data/pvp.db")

    for table in LanguageDB.tables():
        mDataMigrateMap[table] = {
            "language": LanguageDB.get(table, "language", "zh_CN"),
            "tpa": TpaDB.get(table, "Toggle1", "false"),
            "pvp": PvpDB.get(table, "enable", "false")
        }
    
    storage = SQLiteStorage("./data/settings.db")

    for item in mDataMigrateMap:
        storage.create(item)
        storage.insert(item, "language", mDataMigrateMap[item]["language"])
        storage.insert(item, "Tpa_Toggle1", mDataMigrateMap[item]["tpa"])
        storage.insert(item, "Pvp_Enable", mDataMigrateMap[item]["pvp"])
    
    os.remove("./data/language.db")
    os.remove("./data/tpa.db")
    os.remove("./data/pvp.db")
