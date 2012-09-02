clarc
=====

clarc is a free rsync-line command-line utility that synchronizes local file system folder to Amazon S3. Only files that were changed/added since last clarc invocation will be uploaded.

clarc keeps track of changes by storing file metadata in a small local database. It possible to rebuild the database from Amazon S3.

Read more at: http://egorfine.com/clarc/
