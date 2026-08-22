import sqlite3, sys
db = sys.argv[1]
c = sqlite3.connect(db)
tables = [r[0] for r in c.execute("select name from sqlite_master where type='table'")]
print(tables)
for t in tables:
    if 'plugin' in t.lower() or 'market' in t.lower() or 'skill' in t.lower() or 'connector' in t.lower():
        print('---', t)
        for row in c.execute(f"select * from {t} limit 50"):
            print(row)
