import sqlite3, time
from collections import defaultdict

def getTimestamp() -> str:
    return str(int(time.time() * 1000000000))

def UpdateBehaviorEvent(path: str) -> None:
    conn = sqlite3.connect(path)
    cursor = conn.cursor()

    cursor.execute("SELECT key, value FROM Events ORDER BY key")
    rows = cursor.fetchall()
    
    result = defaultdict(dict)

    for key, value in rows:
        if '.' in key:
            timestamp, suffix = key.split('.', 1)
            result[timestamp][suffix] = value

    conn.execute("DROP TABLE IF EXISTS Events")

    conn.execute("""CREATE TABLE IF NOT EXISTS Events (
                 key TEXT PRIMARY KEY, 
                 created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 event_name TEXT,
                 event_time TEXT,
                 event_type TEXT,
                 position_x REAL,
                 position_y REAL,
                 position_z REAL,
                 position_dimension TEXT,
                 event_operable TEXT,
                 event_operable_entity TEXT,
                 event_item TEXT,
                 event_org_count TEXT,
                 event_favored_slot TEXT,
                 event_cause TEXT,
                 event_perm TEXT,
                 event_target TEXT,
                 event_experience TEXT,
                 event_message TEXT,
                 player_name TEXT
                """)
    
    conn.execute("BEGIN TRANSACTION")

    for timestamp, suffix_dict in result.items():
        conn.execute("INSERT INTO Events (key, event_name, event_time, event_type, position_x, position_y, position_z, position_dimension) VALUES (?,?,?,?,?,?,?,?)", (timestamp, suffix_dict['EventName'], suffix_dict['EventTime'], suffix_dict['EventType'], suffix_dict['Position.x'], suffix_dict['Position.y'], suffix_dict['Position.z'], suffix_dict['Position.dimension']))

        if 'EventOperable' in suffix_dict:
            conn.execute("INSERT INTO Events (key, event_operable) VALUES (?,?)", (timestamp, suffix_dict['EventOperable']))
        if 'EventBlockEntity' in suffix_dict:
            conn.execute("INSERT INTO Events (key, event_operable_entity) VALUES (?,?)", (timestamp, suffix_dict['EventBlockEntity']))
        if 'EventOperableEntity' in suffix_dict:
            conn.execute("INSERT INTO Events (key, event_operable_entity) VALUES (?,?)", (timestamp, suffix_dict['EventOperableEntity']))
        if 'EventItem' in suffix_dict:
            conn.execute("INSERT INTO Events (key, event_item) VALUES (?,?)", (timestamp, suffix_dict['EventItem']))
        if 'EventOrgCount' in suffix_dict:
            conn.execute("INSERT INTO Events (key, event_org_count) VALUES (?,?)", (timestamp, suffix_dict['EventOrgCount']))
        if 'EventFavoredSlot' in suffix_dict:
            conn.execute("INSERT INTO Events (key, event_favored_slot) VALUES (?,?)", (timestamp, suffix_dict['EventFavoredSlot']))
        if 'EventCause' in suffix_dict:
            conn.execute("INSERT INTO Events (key, event_cause) VALUES (?,?)", (timestamp, suffix_dict['EventCause']))
        if 'EventSource' in suffix_dict:
            conn.execute("INSERT INTO Events (key, event_cause) VALUES (?,?)", (timestamp, suffix_dict['EventSource']))
        if 'EventPerm' in suffix_dict:
            conn.execute("INSERT INTO Events (key, event_perm) VALUES (?,?)", (timestamp, suffix_dict['EventPerm']))
        if 'EventTarget' in suffix_dict:
            conn.execute("INSERT INTO Events (key, event_target) VALUES (?,?)", (timestamp, suffix_dict['EventTarget']))
        if 'EventExperience' in suffix_dict:
            conn.execute("INSERT INTO Events (key, event_experience) VALUES (?,?)", (timestamp, suffix_dict['EventExperience']))
        if 'EventMessage' in suffix_dict:
            conn.execute("INSERT INTO Events (key, event_message) VALUES (?,?)", (timestamp, suffix_dict['EventMessage']))
        if 'PlayerName' in suffix_dict:
            conn.execute("INSERT INTO Events (key, player_name) VALUES (?,?)", (timestamp, suffix_dict['PlayerName']))

    conn.execute("COMMIT")
    
    conn.close()

def UpdateBlacklist(path: str) -> None:
    conn = sqlite3.connect(path)
    cursor = conn.cursor()

    cursor.execute("SELECT key, value FROM Blacklist ORDER BY key")
    rows = cursor.fetchall()
    
    result = defaultdict(dict)
    
    for key, value in rows:
        if '.' in key:
            timestamp, suffix = key.split('.', 1)
            result[timestamp][suffix] = value

    conn.execute("DROP TABLE IF EXISTS Blacklist")

    conn.execute("""CREATE TABLE IF NOT EXISTS Blacklist (
                 key TEXT PRIMARY KEY, 
                 created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 name TEXT,
                 cause TEXT,
                 time TEXT,
                 subtime TEXT,
                 data_uuid TEXT,
                 data_ip TEXT,
                 data_clientid TEXT
                );""")
                
    
    conn.execute("BEGIN TRANSACTION")

    for timestamp, suffix_dict in result.items():
        conn.execute("INSERT INTO Blacklist (key, name, cause, time, subtime, data_uuid, data_ip, data_clientid) VALUES (?,?,?,?,?,?,?,?)", (timestamp, suffix_dict['NAME'], suffix_dict['CAUSE'], suffix_dict['TIME'], suffix_dict['SUBTIME'], suffix_dict['DATA_UUID'], suffix_dict['DATA_IP'], suffix_dict['DATA_CLIENTID']))
    
    conn.execute("COMMIT")

    conn.close()

def UpdateChat(path: str) -> None:
    conn = sqlite3.connect(path)
    cursor = conn.cursor()

    cursor.execute("SELECT key, value FROM Blacklist ORDER BY key")
    rows = cursor.fetchall()
    
    blacklist_result = defaultdict(dict)
    
    for key, value in rows:
        if '.' in key:
            suffix, targets = key.split('.', 1)
            target, subkey = targets.rsplit('_', 1)
            blacklist_result[suffix][target][subkey] = value

    cursor.execute("SELECT key, value FROM Titles ORDER BY key")
    rows = cursor.fetchall()
    
    titles_result = defaultdict(dict)

    for key, value in rows:
        if '.' in key:
            suffix, title = key.split('.', 1)
            titles_result[suffix][title] = value

    conn.execute("DROP TABLE IF EXISTS Blacklist")
    conn.execute("DROP TABLE IF EXISTS Titles")

    conn.execute("""CREATE TABLE IF NOT EXISTS Blacklist (
                 key TEXT PRIMARY KEY, 
                 created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 name TEXT,
                 target TEXT,
                 author TEXT,
                 time TEXT
                );""")

    conn.execute("""CREATE TABLE IF NOT EXISTS Titles (
                 key TEXT PRIMARY KEY, 
                 created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 title TEXT,
                 author TEXT,
                 time TEXT
                );""")
    
    conn.execute("BEGIN TRANSACTION")

    for suffix, target_dict in blacklist_result.items():
        for target, subkey_dict in target_dict.items():
            conn.execute("INSERT INTO Blacklist (key, name, target, author, time) VALUES (?,?,?,?,?)", (getTimestamp(), subkey_dict['NAME'], target, suffix, subkey_dict['TIME']))
        
    for suffix, title_dict in titles_result.items():
        for title, value in title_dict.items():
            conn.execute("INSERT INTO Titles (key, title, author, time) VALUES (?,?,?,?)", (getTimestamp(), title, suffix, value))

    conn.execute("COMMIT")

    conn.close()

def UpdateMarket(path: str) -> None:
    conn = sqlite3.connect(path)
    cursor = conn.cursor()

    cursor.execute("SELECT key, value FROM Blacklist ORDER BY key")
    rows = cursor.fetchall()
    
    blacklist_result = defaultdict(dict)
    
    for key, value in rows:
        if '.' in key:
            suffix, targets = key.split('.', 1)
            target, subkey = targets.rsplit('_', 1)
            blacklist_result[suffix][target][subkey] = value

    cursor.execute("SELECT key, value FROM Item ORDER BY key")
    rows = cursor.fetchall()

    item_result = defaultdict(dict)

    for key, value in rows:
        if '.' in key:
            suffix, item = key.split('.', 1)
            item_result[suffix][item] = value

    conn.execute("DROP TABLE IF EXISTS Blacklist")
    conn.execute("DROP TABLE IF EXISTS Item")

    conn.execute("""CREATE TABLE IF NOT EXISTS Blacklist (
                 key TEXT PRIMARY KEY, 
                 created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 name TEXT,
                 target TEXT,
                 author TEXT,
                 time TEXT
                );""")

    conn.execute("""CREATE TABLE IF NOT EXISTS Item (
                 key TEXT PRIMARY KEY, 
                 created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 name TEXT,
                 icon TEXT,
                 introduce TEXT,
                 score TEXT,
                 data TEXT,
                 player_name TEXT,
                 player_uuid TEXT
                );""")
    
    conn.execute("BEGIN TRANSACTION")

    for suffix, target_dict in blacklist_result.items():
        for target, subkey_dict in target_dict.items():
            conn.execute("INSERT INTO Blacklist (key, name, target, author, time) VALUES (?,?,?,?,?)", (getTimestamp(), subkey_dict['NAME'], target, suffix, subkey_dict['TIME']))
        
    for suffix, item_dict in item_result.items():
        for item, value_dict in item_dict.items():
            conn.execute("INSERT INTO Item (key, name, icon, introduce, score, data, player_name, player_uuid) VALUES (?,?,?,?,?,?,?,?)", (getTimestamp(), value_dict['NAME'], value_dict['ICON'], value_dict['INTRODUCE'], value_dict['SCORE'], value_dict['DATA'], value_dict['PLAYER_NAME'], value_dict['PLAYER_UUID']))

    conn.execute("COMMIT")

    conn.close()

def UpdateMute(path: str) -> None:
    conn = sqlite3.connect(path)
    cursor = conn.cursor()

    cursor.execute("SELECT key, value FROM Mute ORDER BY key")
    rows = cursor.fetchall()

    result = defaultdict(dict)

    for key, value in rows:
        if '.' in key:
            timestamp, suffix = key.split('.', 1)
            result[timestamp][suffix] = value

    conn.execute("DROP TABLE IF EXISTS Mute")
    
    conn.execute("""CREATE TABLE IF NOT EXISTS Mute (
                 key TEXT PRIMARY KEY, 
                 created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 name TEXT,
                 cause TEXT,
                 time TEXT,
                 subtime TEXT,
                 data TEXT
                );""")
    
    conn.execute("BEGIN TRANSACTION")

    for timestamp, suffix_dict in result.items():
        conn.execute("INSERT INTO Mute (key, name, cause, time, subtime, data) VALUES (?,?,?,?,?,?)", (timestamp, suffix_dict['NAME'], suffix_dict['CAUSE'], suffix_dict['TIME'], suffix_dict['SUBTIME'], suffix_dict['DATA']))
    
    conn.execute("COMMIT")

    conn.close()

def UpdateTpa(path: str) -> None:
    conn = sqlite3.connect(path)
    cursor = conn.cursor()

    cursor.execute("SELECT key, value FROM Blacklist ORDER BY key")
    rows = cursor.fetchall()

    result = defaultdict(dict)
    for key, value in rows:
        if '.' in key:
            suffix, targets = key.split('.', 1)
            target, subkey = targets.rsplit('_', 1)
            result[suffix][target][subkey] = value

    conn.execute("DROP TABLE IF EXISTS Blacklist")

    conn.execute("""CREATE TABLE IF NOT EXISTS Blacklist (
                 key TEXT PRIMARY KEY, 
                 created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 name TEXT,
                 target TEXT,
                 author TEXT,
                 time TEXT
                );""")
    
    conn.execute("BEGIN TRANSACTION")

    for suffix, target_dict in result.items():
        for target, subkey_dict in target_dict.items():
            conn.execute("INSERT INTO Blacklist (key, name, target, author, time) VALUES (?,?,?,?,?)", (getTimestamp(), subkey_dict['NAME'], target, suffix, subkey_dict['TIME']))
    
    conn.execute("COMMIT")

    conn.close()

def UpdateSettings(path: str) -> None:
    conn = sqlite3.connect(path)
    cursor = conn.cursor()

    conn.execute("""CREATE TABLE IF NOT EXISTS Chat (
                 key TEXT PRIMARY KEY, 
                 created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 name TEXT,
                 title TEXT
                );""")
        
    conn.execute("""CREATE TABLE IF NOT EXISTS Language (
                 key TEXT PRIMARY KEY, 
                 created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 name TEXT,
                 value TEXT
                );""")
    
    conn.execute("""CREATE TABLE IF NOT EXISTS Market (
                 key TEXT PRIMARY KEY, 
                 created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 name TEXT,
                 score TEXT
                );""")
    
    conn.execute("""CREATE TABLE IF NOT EXISTS Notice (
                 key TEXT PRIMARY KEY, 
                 created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 name TEXT,
                 close TEXT
                );""")
    
    conn.execute("""CREATE TABLE IF NOT EXISTS Pvp (
                 key TEXT PRIMARY KEY, 
                 created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 name TEXT,
                 enable TEXT
                );""")
    
    conn.execute("""CREATE TABLE IF NOT EXISTS Tpa (
                 key TEXT PRIMARY KEY, 
                 created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 name TEXT,
                 invite TEXT
                );""")

    conn.execute("""CREATE TABLE IF NOT EXISTS Wallet (
                 key TEXT PRIMARY KEY, 
                 created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                 name TEXT,
                 score TEXT
                );""")

    cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name;")
    rows = cursor.fetchall()

    conn.execute("BEGIN TRANSACTION")

    for row in rows:
        uuid = row[0].split("$")[1]
        cursor.execute("SELECT key, value FROM " + row[0] + " ORDER BY key")
        rows = cursor.fetchall()

        result = defaultdict(dict)
        for key, value in rows:
            result[key] = value

        conn.execute("DROP TABLE IF EXISTS " + row[0])

        conn.execute("INSERT INTO Chat (key, name, title) VALUES (?,?,?)", (uuid, result['name'], result['Chat_Title']))
        conn.execute("INSERT INTO Language (key, name, value) VALUES (?,?,?)", (uuid, result['name'], result['language']))
        conn.execute("INSERT INTO Market (key, name, score) VALUES (?,?,?)", (uuid, result['name'], result['Market_Score']))
        conn.execute("INSERT INTO Notice (key, name, close) VALUES (?,?,?)", (uuid, result['name'], result['Notice_Toggle1']))
        conn.execute("INSERT INTO Pvp (key, name, enable) VALUES (?,?,?)", (uuid, result['name'], result['Pvp_Enable']))
        conn.execute("INSERT INTO Tpa (key, name, invite) VALUES (?,?,?)", (uuid, result['name'], result['Tpa_Toggle1']))
        conn.execute("INSERT INTO Wallet (key, name, score) VALUES (?,?,?)", (uuid, result['name'], result['Wallet_Score']))

    conn.execute("COMMIT")

if __name__ == "__main__":
    print("Starting migration.")

    UpdateBehaviorEvent("./data/behaviorevent.db")
    UpdateBlacklist("./data/blacklist.db")
    UpdateChat("./data/chat.db")
    UpdateMarket("./data/market.db")
    UpdateMute("./data/mute.db")
    UpdateTpa("./data/tpa.db")
    UpdateSettings("./data/settings.db")

    print("Migration complete.")