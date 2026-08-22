import sqlite3, sys
db = sys.argv[1]
kw = sys.argv[2].lower()
c = sqlite3.connect(db)
rows = c.execute("select ts, level, target, substr(feedback_log_body,1,300) from logs where lower(feedback_log_body) like ? order by id desc limit 40", ('%'+kw+'%',))
for r in rows:
    print(r)
