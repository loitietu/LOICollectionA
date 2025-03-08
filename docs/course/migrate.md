# 数据迁移
> `数据迁移`是指将数据从一个系统或数据库迁移到另一个系统或数据库的过程。`数据迁移`是数据管理的重要组成部分，可以用于数据备份、数据恢复、数据整合、数据迁移等场景。  
> 而在此的`数据迁移`则是指将`LOICollectionA`中的指定低版本数据迁移到对应的`LOICollectionA`高版本中。

!> 在数据迁移之前请确保已正常安装 `Python 3.10.0` 及以上并配置好服务端

### 对于 1.4.6 版本升至 1.4.7 版本
1. 请在插件根目录下创建 `migrate.py` 文件，并复制以下内容到 `migrate.py` 中
```py
import sqlite3, os
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

    def remove(self, table: str) -> None:
        self.conn.execute(f"DROP TABLE IF EXISTS {table};")
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

    def delete(self, table: str, key: str) -> None:
        self.conn.execute(f"DELETE FROM {table} WHERE key = ?;", (key,))
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

    def tables(self, table: Optional[str] = None) -> List[str]:
        if table:
            cursor = self.conn.execute(f"SELECT key FROM {table};")
        else:
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
```
2. 完成后在命令行中执行 `python migrate.py` 即可完成迁移
3. 迁移完成后，新的 `settings.db` 文件将包含所有迁移的数据

### 对于 1.6.1 版本升至 1.6.2 版本
1. 请在插件根目录下创建 `migrate.py` 文件，并复制以下内容到 `migrate.py` 中
```py
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

    def remove(self, table: str) -> None:
        self.conn.execute(f"DROP TABLE IF EXISTS {table};")
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

    def delete(self, table: str, key: str) -> None:
        self.conn.execute(f"DELETE FROM {table} WHERE key = ?;", (key,))
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

    def tables(self, table: Optional[str] = None) -> List[str]:
        if table:
            cursor = self.conn.execute(f"SELECT key FROM {table};")
        else:
            cursor = self.conn.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;")
        return [row[0] for row in cursor.fetchall()]
    
if __name__ == "__main__":
    mDataMigrateMap = {}
    MarketDB = SQLiteStorage("./data/market.db")
    for table in MarketDB.tables():
        if "Item" in table or "ITEMS" in table or "PLAYER" in table:
            continue
        mDataMigrateMap[table] = MarketDB.get(table, "score", 0)
        MarketDB.remove(table)
    storage = SQLiteStorage("./data/settings.db")
    for item in mDataMigrateMap:
        storage.create(item)
        storage.insert(item, "Score", mDataMigrateMap[item])
```
2. 完成后在命令行中执行 `python migrate.py` 即可完成迁移
3. 迁移完成后，新的 `settings.db` 文件将包含所有迁移的数据
