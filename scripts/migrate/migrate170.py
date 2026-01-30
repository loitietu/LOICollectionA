import re
import copy
import json
import time
import sqlite3
from typing import List, Optional
from collections import defaultdict
 
class SQLiteStorage:
    def __init__(self, db_path: str):
        self.conn = sqlite3.connect(db_path)
        self.conn.execute("PRAGMA journal_mode = MEMORY;")
        self.conn.execute("PRAGMA synchronous = NORMAL;")
        self.conn.execute("PRAGMA temp_store = MEMORY;")
        self.conn.execute("PRAGMA cache_size = 8096;")

    def __del__(self):
        self.conn.close()

    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, _, __):
        if exc_type:
            self.conn.rollback()
        else:
            self.conn.commit()
        self.conn.close()

    def create(self, table: str) -> None:
        self.conn.execute(f"CREATE TABLE IF NOT EXISTS {table} (key TEXT PRIMARY KEY, value TEXT) WITHOUT ROWID;")
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
            cursor = self.conn.execute("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name;")
        return [row[0] for row in cursor.fetchall()]

def getTimestamp() -> str:
    return str(int(time.time() * 1000000000))

def UpdateBlacklist(path: str) -> None:
    DBStorage = SQLiteStorage(path)
    
    tables = DBStorage.tables()
    
    with DBStorage.conn:
        DBStorageMap = []
        
        for table in tables:
            mData = table.replace("OBJECT$", "")
            
            if re.match(r"^[0-9a-f]{8}_[0-9a-f]{4}_[0-9a-f]{4}_[0-9a-f]{4}_[0-9a-f]{12}$", mData):
                mData = mData.replace("_", "-")
                mType = "uuid"
            elif re.match(r"^\d{1,3}_\d{1,3}_\d{1,3}_\d{1,3}$", mData):
                mData = mData.replace("_", ".")
                mType = "ip"
            else:
                mType = "clientid"
            
            DBStorageMap.append({
                "cause": DBStorage.get(table, "cause", "None"),
                "name": DBStorage.get(table, "player", "None"),
                "time": DBStorage.get(table, "time", "None"),
                "subtime": DBStorage.get(table, "subtime", "None"),
                "data": mData,
                "type": mType
            })

        for table in tables:
            DBStorage.remove(table)
        
        DBStorage.create("Blacklist")
        
        for obj in DBStorageMap:
            mTimestamp = getTimestamp()
            
            data_fields = {
                "uuid": (".DATA_UUID", obj["data"]),
                "ip": (".DATA_IP", obj["data"]),
                "clientid": (".DATA_CLIENTID", obj["data"])
            }
            
            DBStorage.conn.execute(f"""
                INSERT INTO Blacklist VALUES 
                (?,?), (?,?), (?,?), (?,?), (?,?), (?,?), (?,?)
            """, (
                f"{mTimestamp}.NAME", obj["name"],
                f"{mTimestamp}.CAUSE", obj["cause"],
                f"{mTimestamp}.TIME", obj["time"],
                f"{mTimestamp}.SUBTIME", obj["subtime"],
                f"{mTimestamp}.DATA_UUID", "",
                f"{mTimestamp}.DATA_IP", "",
                f"{mTimestamp}.DATA_CLIENTID", ""
            ))
            
            field_suffix, value = data_fields[obj["type"]]
            DBStorage.conn.execute(
                f"UPDATE Blacklist SET value = ? WHERE key = ?",
                (value, f"{mTimestamp}{field_suffix}")
            )

def UpdateMute(path: str) -> None:
    DBStorage = SQLiteStorage(path)
    
    tables = DBStorage.tables()
    
    with DBStorage.conn:
        DBStorageMap = []
        
        for table in tables:
            mData = table.replace("OBJECT$", "")
            
            DBStorageMap.append({
                "cause": DBStorage.get(table, "cause", "None"),
                "name": DBStorage.get(table, "player", "None"),
                "time": DBStorage.get(table, "time", "None"),
                "subtime": DBStorage.get(table, "subtime", "None"),
                "data": mData.replace("_", "-")
            })

        for table in tables:
            DBStorage.remove(table)
        
        DBStorage.create("Mute")
        
        for obj in DBStorageMap:
            mTimestamp = getTimestamp()
            
            DBStorage.conn.execute(f"""
                INSERT INTO Mute VALUES 
                (?,?), (?,?), (?,?), (?,?), (?,?)
            """, (
                f"{mTimestamp}.NAME", obj["name"],
                f"{mTimestamp}.CAUSE", obj["cause"],
                f"{mTimestamp}.TIME", obj["time"],
                f"{mTimestamp}.SUBTIME", obj["subtime"],
                f"{mTimestamp}.DATA", obj["data"]
            ))

def UpdateTpa(path: str) -> None:
    DBStorage = SQLiteStorage(path)
    
    with DBStorage.conn:
        DBStoargeMap = defaultdict(lambda: {
            "blacklists": []
        })
        
        for table in DBStorage.tables():
            if table.endswith("$PLAYER"):
                mData = table.replace("OBJECT$", "").replace("$PLAYER", "")
                keys = DBStorage.tables(table)
                for key in keys:
                    DBStoargeMap[mData]["blacklists"].append({
                        "data": key,
                        "name": DBStorage.get(f"{table}{key}", "name", ""),
                        "time": DBStorage.get(f"{table}{key}", "time", "")
                    })
                    
        for table in DBStorage.tables():
            DBStorage.remove(table)
        
        DBStorage.create("Blacklist")

        blacklist_batch = []

        for mData, data in DBStoargeMap.items():
            for bl in data["blacklists"]:
                prefix = f"{mData}.{bl["data"]}"
                blacklist_batch.extend([
                    (f"{prefix}_NAME", bl["name"]),
                    (f"{prefix}_TIME", bl["time"])
                ])

        if blacklist_batch:
            DBStorage.conn.executemany(
                "INSERT INTO Blacklist VALUES (?, ?)",
                blacklist_batch
            )

def UpdateChat(path: str, path1: str) -> None:
    with SQLiteStorage(path) as DBStorage, SQLiteStorage(path1) as DBStorage1:
        DBStoargeMap = defaultdict(lambda: {
            "titles": [],
            "blacklists": [],
            "title": ""
        })

        for table in DBStorage.tables():
            if table.endswith("$TITLE"):
                mData = table.replace("OBJECT$", "").replace("$TITLE", "")
                keys = DBStorage.tables(table)
                for key in keys:
                    DBStoargeMap[mData]["titles"].append({
                        "name": key,
                        "time": DBStorage.get(table, key, "")
                    })
                    
            elif table.endswith("$CHAT"):
                mData = table.replace("OBJECT$", "").replace("$CHAT", "")
                keys = DBStorage.tables(table)
                for key in keys:
                    DBStoargeMap[mData]["blacklists"].append({
                        "data": key,
                        "name": DBStorage.get(f"{table}{key}", "name", ""),
                        "time": DBStorage.get(f"{table}{key}", "time", "")
                    })
                    
            else:
                mData = table.replace("OBJECT$", "")
                DBStoargeMap[mData]["title"] = DBStorage.get(table, "title", "")

        DBStorage.conn.execute("BEGIN")

        for table in DBStorage.tables():
            DBStorage.conn.execute(f"DROP TABLE IF EXISTS {table}")
        
        DBStorage.create("Blacklist")
        DBStorage.create("Titles")

        blacklist_batch = []
        titles_batch = []
        settings_batch = []
        
        for mData, data in DBStoargeMap.items():
            for bl in data["blacklists"]:
                prefix = f"{mData}.{bl["data"]}"
                blacklist_batch.extend([
                    (f"{prefix}_NAME", bl["name"]),
                    (f"{prefix}_TIME", bl["time"])
                ])
            
            for title in data["titles"]:
                titles_batch.append((f"{mData}.{title["name"]}", title["time"]))
            
            settings_batch.append((f"OBJECT${mData}", "Chat_Title", data["title"]))

        if blacklist_batch:
            DBStorage.conn.executemany(
                "INSERT INTO Blacklist VALUES (?, ?)",
                blacklist_batch
            )
        if titles_batch:
            DBStorage.conn.executemany(
                "INSERT INTO Titles VALUES (?, ?)",
                titles_batch
            )
            
        seen_tables = set()
        for table, key, value in settings_batch:
            if table not in seen_tables:
                DBStorage1.create(table)
                seen_tables.add(table)
            DBStorage1.conn.execute(
                "INSERT OR REPLACE INTO {} VALUES (?, ?)".format(table),
                (key, value)
            )

        DBStorage.conn.commit()
        DBStorage1.conn.commit()

def UpdateMarket(path: str, path1: str) -> None:
    with SQLiteStorage(path) as DBStorage, SQLiteStorage(path1) as DBStorage1:
        DBStorageMap = defaultdict(lambda: {
            "items": [],
            "blacklists": []
        })
        for table in DBStorage.tables():
            if table.endswith("$PLAYER"):
                mData = table.replace("OBJECT$", "").replace("$PLAYER", "")
                keys = DBStorage.tables(table)
                for key in keys:
                    DBStorageMap[mData]["blacklists"].append({
                        "data": key,
                        "name": DBStorage.get(f"{table}{key}", "name", ""),
                        "time": DBStorage.get(f"{table}{key}", "time", "")
                    })
                
            elif table.endswith("$ITEMS"):
                mData = table.replace("OBJECT$", "").replace("$ITEMS", "")
                keys = DBStorage.tables(table)
                for key in keys:
                    mId = f"OBJECT${mData}$ITEMS_$LIST_{key}"

                    DBStorageMap[mData]["items"].append({
                        "name": DBStorage.get(mId, "name", "None"),
                        "icon": DBStorage.get(mId, "icon", "None"),
                        "introduce": DBStorage.get(mId, "introduce", "None"),
                        "score": DBStorage.get(mId, "score", "0"),
                        "nbt": DBStorage.get(mId, "nbt", "None"),
                        "player_name": DBStorage.get(mId, "player", "None"),
                        "player_uuid": mData.replace("_", "-")
                    })
        
        DBStorage.conn.execute("BEGIN")

        for table in DBStorage.tables():
            DBStorage.conn.execute(f"DROP TABLE IF EXISTS {table}")

        DBStorage.create("Blacklist")
        DBStorage.create("Item")

        blacklist_batch = []
        item_batch = []

        for key, value in DBStorageMap.items():
            for bl in value["blacklists"]:
                prefix = f"{key}.{bl['data']}"
                blacklist_batch.extend([
                    (f"{prefix}_NAME", bl["name"]),
                    (f"{prefix}_TIME", bl["time"])
                ])
            
            for item in value["items"]:
                mTimestamp = getTimestamp()

                item_batch.extend([
                    (f"{mTimestamp}.NAME", item["name"]),
                    (f"{mTimestamp}.ICON", item["icon"]),
                    (f"{mTimestamp}.INTRODUCE", item["introduce"]),
                    (f"{mTimestamp}.SCORE", item["score"]),
                    (f"{mTimestamp}.DATA", item["nbt"]),
                    (f"{mTimestamp}.PLAYER_NAME", item["player_name"]),
                    (f"{mTimestamp}.PLAYER_UUID", item["player_uuid"])
                ])

        if blacklist_batch:
            DBStorage.conn.executemany(
                "INSERT INTO Blacklist VALUES (?, ?)",
                blacklist_batch
            )
        if item_batch:
            DBStorage.conn.executemany(
                "INSERT INTO Item VALUES (?, ?)",
                item_batch
            )

        for table in DBStorage1.tables():
            DBStorage1.insert(table, "Market_Score", DBStorage1.get(table, "Score", "0"))
            DBStorage1.delete(table, "Score")
        
        DBStorage.conn.commit()
        DBStorage1.conn.commit()

def UpdateCdk(path: str) -> None:
    with open(path, "r") as f:
        data = json.load(f)

        mNewData = copy.deepcopy(data)

        for key, value in data.items():
            mNewData[key]["item"] = []

            for k, v in value["item"].items():
                if v["type"] == "universal":
                    mNewData[key]["item"].append({
                        "id": k,
                        "name": v["name"],
                        "quantity": v["quantity"],
                        "specialvalue": v["specialvalue"],
                        "type": "universal"
                    })
                else:
                    mNewData[key]["item"].append({
                        "id": k,
                        "type": "nbt"
                    })

        with open(path, "w") as f1:
            json.dump(mNewData, f1, indent=4)

if __name__ == "__main__":
    print("Starting migration.")

    UpdateBlacklist("./data/blacklist.db")
    UpdateMute("./data/mute.db")
    UpdateTpa("./data/tpa.db")
    UpdateChat("./data/chat.db", "./data/settings.db")
    UpdateMarket("./data/market.db", "./data/settings.db")

    UpdateCdk("./config/cdk.json")

    print("Migration complete.")

