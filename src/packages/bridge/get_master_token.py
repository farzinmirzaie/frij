#!/usr/bin/env python3
"""One-time: turn a Google OAuth token into a gkeepapi *master token*.

gkeepapi can no longer log in with a plain password; it needs a long-lived
"master token". You mint it once, store it as a secret, and the bridge reuses it.

Steps (Chrome):
 1. Open an Incognito window. Go to https://accounts.google.com/EmbeddedSetup
 2. Sign in; accept the consent prompt. **The page will then hang / go blank —
    that is expected.** The token cookie is already set at that point; you don't
    need the page to finish loading.
 3. Open DevTools (Cmd+Opt+I) -> Application -> Cookies ->
    https://accounts.google.com. Copy the value of the `oauth_token` cookie
    (it starts with `oauth2_4/`).
 4. Run this script and paste the email + that oauth_token *right away* — the
    token is single-use and expires within minutes.

The printed MASTER TOKEN is as powerful as the account password — store it only
as a secret (GitHub Actions secret / local .env), never commit it.
"""
import sys

try:
    import gpsoauth
except ImportError:
    sys.exit("install deps first:  pip install -r requirements.txt")

email = input("Google email: ").strip()
oauth_token = input("oauth_token (oauth2_4/...): ").strip()
android_id = input("Android ID [0123456789abcdef]: ").strip() or "0123456789abcdef"

res = gpsoauth.exchange_token(email, oauth_token, android_id)
token = res.get("Token")
if not token:
    if res.get("Error") == "BadAuthentication":
        sys.exit(
            "BadAuthentication — the oauth_token was expired or already used.\n"
            "These are single-use and last only a couple of minutes. Fix:\n"
            "  1. Close the Incognito window; open a FRESH one.\n"
            "  2. Re-do https://accounts.google.com/EmbeddedSetup, sign in, accept.\n"
            "  3. Copy the NEW `oauth_token` cookie and run this again right away.\n"
            "If it keeps failing, the account likely has 2FA/Advanced Protection — "
            "tell the dev and we'll switch to the app-password flow."
        )
    sys.exit(f"failed to exchange token: {res}")

print("\n--- store GKEEP_MASTER_TOKEN as a secret, keep it private ---")
print(f"GKEEP_MASTER_TOKEN={token}")
# Printed for reference only — the sync doesn't use it (gkeepapi derives its
# own device id), so there's no need to store GKEEP_DEVICE_ID.
print(f"GKEEP_DEVICE_ID={android_id}  # not required by the sync")
