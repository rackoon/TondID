# Domain plan

This repository is ready to support the following domain structure:

- `tondid.com` as the primary canonical domain
- `www.tondid.com` as a redirect to the canonical domain
- `tondid.eu` as an EU-facing redirect or later as a regional entry point

## Recommended first rollout

1. Point `tondid.com` to the public product site.
2. Redirect `www.tondid.com` to `tondid.com`.
3. Redirect `tondid.eu` to `tondid.com`.

This keeps the product identity concentrated under one canonical domain while preserving the EU domain for later regional use.

## Suggested future hostnames

- `app.tondid.com` for a customer or operator-facing web application
- `docs.tondid.com` for public documentation
- `status.tondid.com` for operational status or public monitoring

## What still requires an external account action

The repository and branding are prepared locally and in GitHub, but live domain binding still requires:

- access to the registrar or DNS provider where `tondid.com` and `tondid.eu` are managed
- a chosen hosting target for the public site
- creation of the actual DNS records

Once the registrar or DNS provider is known, the remaining work is to create the exact records there and verify propagation.
