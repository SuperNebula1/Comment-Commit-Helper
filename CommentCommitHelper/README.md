# Comment Commit Helper 1.3.4

Unreal Engine 5.8 editor plugin that builds commit/changelist text from Blueprint comments that are new or changed compared with the latest submitted source-control revision.

## Install

Copy the `CommentCommitHelper` folder into:

`YourProject/Plugins/CommentCommitHelper`

Then rebuild your Editor target.

If replacing an older build, delete these first if they exist:

- `Plugins/CommentCommitHelper/Binaries`
- `Plugins/CommentCommitHelper/Intermediate`

## Use

Open:

**Tools -> Comment Commit Helper**

Buttons:

- **Scan Source Control** - compares current Blueprint comments to source-control history
- **Copy Commit Message** - copies the generated text
- **Open Settings** - opens the plugin's Project Settings page

Settings are also available manually at:

**Edit -> Project Settings -> Plugins -> Comment Commit Helper**



## 1.3.1 Build Fix

Fixed literal `\t` sequences accidentally written into the 1.3.0 C++ source. No intended feature behavior changed.

## Leading @ Tag Handling

Version 1.3 adds automatic stripping of a leading `@tag` from generated commit messages.

Examples:

```text
@commit Added stamina regeneration
@fix Fixed sprint collision
@ui Updated player HUD
```

generate:

```text
- Added stamina regeneration
- Fixed sprint collision
- Updated player HUD
```

The tag must be at the beginning of the comment. An `@` appearing later in normal text is not changed.

Supported tag characters are:

- letters
- numbers
- underscore

Examples that are stripped:

```text
@commit
@fix
@ui2
@network_refactor
```

This behavior can be enabled or disabled under:

**Project Settings -> Plugins -> Comment Commit Helper -> Comments -> Output -> Strip Leading @ Tag From Output**

The **Remove Commit Comments** button remains separate. It only removes comments beginning with the configured **Commit Cleanup Prefix**, which defaults to `@commit`.


## Remove Commit Comments

The plugin window includes **Remove Commit Comments**.

By default, it targets Blueprint comments beginning with:

`@commit`

Behavior:

- `@commit` comment boxes are deleted completely.
- `@commit` node comment bubbles have their comment text cleared.
- A confirmation dialog appears first.
- Cleanup uses Unreal's transaction system, so it can be undone with **Ctrl+Z**.
- Changed Blueprints are marked dirty but are not automatically saved.
- The prefix is configurable under:
  **Project Settings -> Plugins -> Comment Commit Helper -> Comments -> Cleanup -> Commit Cleanup Prefix**

Example workflow:

1. Add `@commit Added stamina regeneration`
2. Scan Source Control
3. Copy/use the generated commit message
4. Click **Remove Commit Comments**
5. Save the affected Blueprints


## Configurable settings

### Comments

- Include Comment Boxes
- Include Node Comment Bubbles

### Comment include filters

**Required Comment Prefixes**

Leave empty to include all comments that pass ignore filters.

Example:

`@commit`

When this list contains `@commit`, only comments such as:

`@commit Added stamina regeneration`

are included.

When **Strip Required Prefix From Output** is enabled, the output becomes:

`- Added stamina regeneration`

### Comment ignore filters

- Ignored Comment Prefixes
- Ignored Comment Text Contains
- Ignored Exact Comments
- Ignore Unreal Disabled Node Messages

The plugin automatically ignores Unreal's common generated message:

`This node is disabled and will not be called. Drag off pins to build functionality.`

The broader **Ignore Unreal Disabled Node Messages** toggle also filters variants containing:

`This node is disabled and will not be called`

Default ignored prefixes:

- `TODO:`
- `DEV:`
- `DEBUG:`

You can remove or change these in Project Settings.

### Asset include/ignore filters

- Included Asset Paths
- Ignored Asset Paths
- Ignored Asset Name Prefixes

Examples:

Included path:
`/Game/Blueprints/`

Ignored path:
`/Game/ThirdParty/`

Ignored asset prefix:
`TEST_`

### Source control

- Include Newly Added Assets
- Include Modified Assets

## Comparison behavior

Filters are applied to both the current Blueprint and the historical source-control Blueprint. This prevents an ignored old comment from being mistaken for a new one.

For modified assets, the plugin asks Unreal's active source-control provider for revision history, downloads the latest submitted revision, loads the historical `.uasset`, and compares Blueprint graph comments.

For newly added assets with no historical revision, included comments are treated as new.

## Output example

```text
BP_Player
- Added stamina regeneration
- Fixed sprint speed calculation

WBP_PlayerHUD
- Added low stamina warning
```

## Current scope

Supported:

- Blueprint comment boxes
- visible Blueprint node comment bubbles
- editor-configurable comment filters
- editor-configurable asset filters
- generic Unreal Source Control providers that support history/revision retrieval

Not yet supported:

- Material graph comments
- Niagara comments
- automatically writing/submitting the changelist description

## Fab Technical Review Compliance (1.3.4)

This release adds the Fab Technical Review requirements requested during review:

- `EngineVersion` is explicitly set to `5.8.0`.
- Every module declares `PlatformAllowList: ["Win64"]`.
- Every C++ source/header file includes a 2026 Tanner copyright notice.
- The module rules file also includes the same copyright notice for consistency.

The Fab listing for this build should list **Win64** as the supported target platform.


## 1.3.4

Updated all source/header file headers for Fab review.


## 1.3.4 Fab Review Fix

- Added explicit copyright notices to all source/header/build files.
- Added Config/FilterPlugin.ini to include the root README.md in distribution.
