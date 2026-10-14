(config
  (service "serde")
  (retries 3)
  (enabled true)
  (endpoints (primary "https://example.invalid") (backup "https://backup.invalid")))
