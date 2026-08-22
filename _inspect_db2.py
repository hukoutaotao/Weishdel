import sqlite3, sys
db = sys.argv[1]
c = sqlite3.connect(db)
tables = [r[0] for r in c.execute("select name from sqlite_master where type='table'")]
print('TABLES:', tables)
for t in tables:
    cols = [d[1] for d in c.execute(f"PRAGMA table_info({t})")]
    print('===', t, cols)
    try:
        for row in c.execute(f"select * from {t} where 1=0"):
            pass
    except Exception as e:
        print('err', e)
