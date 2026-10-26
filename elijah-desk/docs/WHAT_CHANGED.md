# What changed, and why

Optimized draft of the Elijah weekly content desk. Week of 26 Oct – 1 Nov 2026. Research snapshot 25 Oct; time-sensitive items rechecked in a browser on the evening of 25 Oct. The original snapshot (`01_Site_Snapshot`) and the live draft were not touched.

## The biggest improvements

1. **The week reads first, and it is filmable.** Five of seven posts are "film now" (everything needed is on his shelf or his phone). The original week had three posts that depended on a sample, a before/after pair or an AI tracking test. Those are now "verify first" or reserve, with the one post that still carries a dependency (Saturday's AI poll) given a tracked test gate and a fallback.
2. **Twenty-seven ideas became seven posts and a bank of thirteen.** Four sampler comments are one Monday post. Two winter comments are one Wednesday post. The spray-count comment is a beat inside Friday's this-or-that. The two LV questions are one verify-first follow-up. The shelf-frost AI idea was removed (decoration without a payoff). Every original card is still on the page with its decision and where it went.
3. **The dupe conversation gets a personal post, not a re-announcement.** Elijah already posted on Noel (290.1K on Oct 25). Thursday turns one viewer's comment ("the dupe made me buy the original") into Elijah's own buying story. No company named, nothing to verify, rides the week's biggest topic.
4. **Evidence was re-verified today, not carried over.** Sephora listing P526132: $98, 14 vials, the full eligible-size list, Canada/Kohl's excluded, final sale, certificate valid until 8/31/36 (so the Prada "why is it cheaper" math is now exact: the eligible size is 1 oz). Noel's account still unavailable. Elijah's gold reveal 765.2K (14x his monthly median of ~53K). TikHub tried again with the supplied key: still 403/1010.
5. **Creator comparisons are real, dated and relative.** Posting dates are read from the timestamp embedded in each TikTok video ID (checked against Elijah's own known posts), and each post is compared with that creator's own recent grid median at the Oct 25 capture. That is how FBFragrances' "Justice for Noel" (1.6M, 15x), perfumerism's jargon explainer (612K, 10.8x), CurlyScents' this-or-that (6.8x) and Fragrance knowledge's TikTok Shop deal post (70x) were found, and how a counter-example (JurysFragrance's "UNDER ATTACK" headline at 1.1x) made the cut. Ads and pinned posts are flagged. These feed all three sections and produced two new reserve ideas (the real-accounts PSA, the blind showdown).
6. **Elijah's own baseline is on the page.** Median ~53K across 56 posts (Sep 22–Oct 22). The week leans on what has worked for him this month: comment answers, sampler/shopping utility, object-scale AI spectacle, recognizable names (JPG, Azzaro). His Oct 24–25 small-brand reviews sit at or below median, which supports that choice.
7. **An AI routine, in six steps.** Comment mining, watchlist deltas, topic verification, one labeled AI remix a week, hooks from his own transcript, and where views turn into money. Each step is under 30 minutes and keeps the videos looking like Elijah.
8. **Hook stacks (new, from the owner's note).** One comment on screen is a thin hook; five real ones on the same question read as demand. Each post in the week now has a "Hook stack" panel: the real comments already captured on that topic, a stacked hook graphic rendered from those crops (`tools/comment_stack.py`, transparent 1080×1920 PNG, kept inside the TikTok-safe band), and a one-line route to find more (`tools/find_comments.py`, or screenshots from Elijah's phone dropped into `site/assets/found/<post id>/`; `build.py` picks them up). Monday, Tuesday and Wednesday have stacks already. Deliberately not done: generating comments with Nano Banana. Invented viewer comments are fabricated social proof; the handoff rules bar it and it is the kind of thing that gets a creator exposed. Nano Banana stays on bottles and plates.
9. **Simpler page.** Idea → why now → evidence preview → what I need to film, then an expandable brief. One search box and one scope selector. Screenshots enlarge; briefs copy; print expands everything. No scoring widgets.

## Files in this folder

- `site/` — the working revised site (open `site/index.html`; all assets are inside `site/assets`).
- `data.json` — single source of truth for the revised content; `build.py` regenerates the site data and the markdown below from it.
- `IDEA_AUDIT.md` — all 27 ideas with keep / combine / reserve / verify-first / remove and a reason.
- `REVISED_WEEK.md` — the seven posts with premise, opening, why now, evidence, beats, needs, editor notes, effort estimate, verification and backup; plus the reserve bank.
- `HANDOFF_BRIEFS.md` — the same seven, cut down to what Elijah reads and what the editor reads.
- `research/tikhub_status_2026-10-25.json` — the denied TikHub request (no credential inside).
- `research/browser_rechecks_2026-10-25.json` — what was rechecked today and the result.
- `research/creator_comparisons.json` — the watchlist comparisons with dates, counts and medians.
- `tools/find_comments.py` — captures real comments matching keywords under Elijah's own posts (when TikTok shows the comment panel); `tools/comment_stack.py` — stacks real comment crops into one hook graphic.
- `site/assets/stacks/` — rendered hook stacks; `site/assets/found/` — where extra real comment crops go.

## Still needed from the owner

- The genuine green source frame and the gold output from "Chat is this rare?" (Tuesday's before/after). Not in the handoff; Tuesday works without it.
- The untouched can-opening take with handles (Saturday's three-finish test).
- Whether Elijah has the current P526132 kit or only the earlier one.
- The exact bottle in jeymi.70's photo (confirm the variant before Wednesday).
- Which affordable/original pairs Elijah actually owns (Thursday).
- A boutique answer on LV refill price and receipt rules before that follow-up.
- Samples for Explorer Infinite and Lattafa Fearless, if wanted.
- The owner's final creator watchlist (the 30-creator bank is the starting set).

## What was not done, on purpose

- No image or video was generated; no credits spent.
- No TikHub workaround; the API denial is recorded as a denial.
- Nothing deployed, shared, messaged or scheduled.
- Three TikTok videos could not be opened in the research browser (login-gated): Elijah's Oct 23 post at 248.7K, and FBFragrances' "Justice for Noel". Their counts come from the profile grids.
