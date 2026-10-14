(config
  (server (host "localhost") (port 8080))
  (database (engine "postgres") (pool-size 20))
  (features
    (search true)
    (caching false)
    (export-formats "pdf" "html" "epub")))
