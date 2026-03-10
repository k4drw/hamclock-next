# Security Policy

## Supported Versions

Only the latest release of HamClock-Next receives security fixes.

| Version | Supported |
|---------|-----------|
| Latest beta / release | ✓ |
| Older betas | ✗ |

## Reporting a Vulnerability

Please **do not** open a public GitHub issue for security vulnerabilities.

Report vulnerabilities privately by emailing the maintainer directly, or by using
[GitHub Private Security Advisories](https://github.com/features/security/advisories)
on this repository.

Include:
- A description of the vulnerability and its potential impact
- Steps to reproduce or a proof-of-concept
- Affected version(s)
- Any suggested mitigations

You can expect an acknowledgement within **5 business days** and a resolution
timeline within **30 days** for confirmed issues.

## Scope

In scope:
- Buffer overflows or memory corruption in network-facing code (REST API, DX Cluster, CAT/rotator sockets)
- SQL injection via the embedded SQLite database
- Path traversal or arbitrary file write via the REST API
- Authentication bypass (if authentication is added in future)

Out of scope:
- Denial-of-service against the embedded HTTP server (it is single-user, LAN-facing by design)
- Issues requiring local shell access to the host machine
- Cosmetic or non-security rendering bugs

## Security Architecture Notes

- The REST API (`WebServer`) is designed for **local network use only**; do not expose it to the public internet without a reverse proxy with authentication.
- All SQL parameters are bound via prepared statements (`sqlite3_bind_*`); string concatenation is not used in database queries.
- Network data from DX Cluster, NOAA, and other providers is parsed defensively with bounds checks.
