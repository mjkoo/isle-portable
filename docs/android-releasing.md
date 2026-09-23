# Releasing the Android build

Pushing a tag that starts with `v` runs `.github/workflows/android-release.yml`. It builds the
four per-ABI APKs and the universal one, signs them with the release key, checks each signature,
and publishes them as a GitHub Release named after the tag. Its notes open with the credit and
license text in `.github/release-notes.md`, followed by GitHub's generated changelog. The same
workflow can be started by hand from the Actions tab for a tag that already exists, which
is how a failed release is retried.

## The signing key

Android accepts an update only when it is signed with the same key as the installed app. **Lose
the key or change it and every player has to uninstall, losing their saves, before a new release
will install.** Generate it once and keep a backup somewhere other than GitHub:

```sh
keytool -genkeypair -v -keystore release.keystore -alias isle \
  -keyalg RSA -keysize 4096 -validity 36500
```

## The `release` environment

In the repository's Settings > Environments, create an environment named `release`, and add
these four secrets to it:

| Secret | Value |
| - | - |
| `SIGNING_STORE_BASE64` | `base64 < release.keystore`, on one line |
| `SIGNING_STORE_PASSWORD` | the keystore password |
| `SIGNING_KEY_ALIAS` | the alias, `isle` above |
| `SIGNING_KEY_PASSWORD` | the key password, the keystore password unless you gave it its own |

Under Deployment branches and tags, choose **Selected branches and tags** and add a tag rule for
`v*`. Only the release job names this environment, so the CI workflow never sees the key, and
with the rule a workflow run from any other ref cannot reach it either. The job refuses to run
if any of the four secrets is missing, because without them Gradle produces an unsigned APK
rather than failing.

## Cutting a release

```sh
git tag v0.1.0
git push origin v0.1.0
```

The APKs' version code is the commit count and the version name is the project version, the
commit count and the commit, as described in [installing the Android build](android-install.md);
the tag names the release but does not change either. Two tags on the same commit therefore
produce APKs with the same version.

Push only your own tags. A clone of upstream carries upstream's rolling `continuous` tag, which
names a release in upstream's repository and none in this one.
