(() => {
  'use strict';
  const data = window.WEEKLY_DATA;
  const intel = window.INTEL || {};
  const $ = (selector) => document.querySelector(selector);
  const el = (tag, className, text) => {
    const node = document.createElement(tag);
    if (className) node.className = className;
    if (text !== undefined && text !== null) node.textContent = String(text);
    return node;
  };
  if (!data) {
    $('#comments-grid').append(el('p', 'card-body', 'The research file could not load. Refresh the page or open the complete site folder.'));
    $('#source-note').textContent = 'This draft requires data.js alongside the page.';
    return;
  }
  const meta = data.meta || {};
  const lanes = { comments: Array.isArray(data.comments) ? data.comments : [], ai: Array.isArray(data.ai) ? data.ai : [], topics: Array.isArray(data.topics) ? data.topics : [] };
  const laneNames = { comments: 'From comments', ai: 'AI remix', topics: 'Community topic', creators: 'Other creators', reddit: 'Reddit this week' };
  const cardRecords = [];
  const dialog = $('#image-dialog');
  let lastImageTrigger = null;
  let toastTimer;

  function safeLink(value) {
    try { const url = new URL(String(value)); return ['https:', 'http:'].includes(url.protocol) ? url.href : null; } catch { return null; }
  }
  function safeImage(value) {
    const path = String(value || '');
    return /^\.?\/?assets\//.test(path) && !path.includes('..') ? path : null;
  }
  function sourceLink(title, url) {
    const href = safeLink(url);
    if (!href) return el('span', '', title || 'Source unavailable');
    const link = el('a', '', `${title || 'Open source post'} ↗`);
    link.href = href; link.target = '_blank'; link.rel = 'noopener noreferrer';
    return link;
  }
  function badge(text, kind) { return el('span', `badge${kind ? ` ${kind}` : ''}`, text); }
  function list(value) { return Array.isArray(value) ? value : value ? [value] : []; }
  function detailField(parent, title, text) {
    if (!text) return;
    parent.append(el('span', 'field-label', title), el('p', '', text));
  }
  function detailsList(parent, title, values, ordered) {
    const items = list(values).filter(Boolean);
    if (!items.length) return;
    parent.append(el('span', 'field-label', title));
    const group = el(ordered ? 'ol' : 'ul');
    items.forEach((text) => group.append(el('li', '', typeof text === 'string' ? text : JSON.stringify(text))));
    parent.append(group);
  }
  function notify(message) {
    const toast = $('#toast'); toast.textContent = message; toast.hidden = false;
    window.clearTimeout(toastTimer); toastTimer = window.setTimeout(() => { toast.hidden = true; }, 3500);
  }
  function openImage(path, title, caption, trigger, link, linkLabel) {
    lastImageTrigger = trigger;
    $('#dialog-image').src = path; $('#dialog-image').alt = title;
    $('#image-dialog-title').textContent = title;
    $('#dialog-image-caption').textContent = caption;
    const original = $('#dialog-original'); const href = link ? safeLink(link) : null;
    original.href = href || path; original.textContent = href ? `${linkLabel || 'Open the post'} ↗` : 'Open original image ↗';
    dialog.showModal(); $('#close-image').focus();
  }
  function imagePreview(item, lane) {
    const path = safeImage(lane === 'comments' ? item.screenshot : item.image);
    if (!path) return null;
    const button = el('button', 'screenshot-button'); button.type = 'button';
    const title = lane === 'comments' ? `Real comment: ${item.title}` : `Source reference: ${item.title}`;
    button.setAttribute('aria-label', `Enlarge ${title}`);
    const image = el('img'); image.src = path; image.alt = title; image.loading = 'lazy'; image.decoding = 'async';
    button.append(image, el('span', 'image-tag', lane === 'comments' ? 'Real viewer comment' : lane === 'creators' ? 'Profile grid · Oct 25' : lane === 'reddit' ? 'Top of the week · Oct 25' : 'Source reference'), el('span', 'expand-image', '↗'));
    button.addEventListener('click', () => openImage(path, title, lane === 'comments' ? item.source_title || 'User comment on the linked original video.' : item.original_title || item.caption_line || item.title, button, item.dialog_link, item.dialog_link_label));
    image.addEventListener('error', () => { button.hidden = true; });
    return button;
  }
  function briefText(item, lane) {
    const lines = [item.title, `Lane: ${laneNames[lane]}`, item.quote ? `Real comment: “${item.quote}”${item.author ? ` — ${item.author}` : ''}` : '', item.hook ? `Hook: ${item.hook}` : '', item.change ? `Transformation: ${item.change}` : '', item.angle ? `Angle: ${item.angle}` : '', item.why ? `Why: ${item.why}` : '', ...list(item.beats).map((beat, index) => `${index + 1}. ${typeof beat === 'string' ? beat : JSON.stringify(beat)}`), item.raw_needed ? `Raw footage: ${item.raw_needed}` : '', item.production ? `Production: ${item.production}` : '', item.publish_gate ? `Publishing gate: ${item.publish_gate}` : '', ...list(item.notes).map((note) => `Note: ${note}`), item.source_url ? `Source: ${item.source_url}` : '', ...list(item.sources).map((source) => `${source.title}: ${source.url}`)];
    return lines.filter(Boolean).join('\n\n');
  }
  async function copyText(text, button, label) {
    try { await navigator.clipboard.writeText(text); button.textContent = 'Copied ✓'; notify(`${label} copied.`); window.setTimeout(() => { button.textContent = `Copy ${label.toLowerCase()}`; }, 2200); }
    catch { const selection = document.createElement('textarea'); selection.value = text; selection.setAttribute('aria-label', 'Text to copy'); selection.style.position = 'fixed'; selection.style.left = '-9999px'; document.body.append(selection); selection.select(); const copied = document.execCommand('copy'); selection.remove(); button.focus(); notify(copied ? `${label} copied.` : 'Copy is unavailable here. Open the brief to select its text.'); }
  }
  function copyBrief(item, lane, button) {
    const text = briefText(item, lane) + '\n\nCreative direction: minimal pop-ups, purposeful zooms, real product footage. No captions in V1/V2; clean captions allowed in V3. AI concepts must be identified honestly.';
    return copyText(text, button, 'Brief');
  }
  function cardBase(item, lane) {
    const card = el('article', `idea-card ${lane}-card`); card.id = String(item.id);
    const image = imagePreview(item, lane); if (image) card.append(image);
    const body = el('div', 'card-body'); card.append(body);
    const chips = el('div', 'card-meta');
    if (item.priority) chips.append(badge(item.priority, 'priority'));
    if (lane === 'topics') {
      const verified = item.verified;
      if (typeof verified === 'boolean') chips.append(badge(verified ? 'Verified development' : 'Needs checking', verified ? 'fact' : 'check'));
      else if (verified) chips.append(badge(verified, /unverified|needs|check|pending|not verified/i.test(verified) ? 'check' : 'fact'));
    }
    if (chips.childElementCount) body.append(chips);
    body.append(el('h3', '', item.title));
    return { card, body, chips };
  }
  function finishCard(item, lane, card, body, content, summaryLabel) {
    if (Array.isArray(item.attachments)) {
      const extras = el('div', 'source-block'); extras.append(el('span', 'field-label', 'Additional evidence'));
      item.attachments.forEach((attachment) => {
        const path = safeImage(attachment.path); if (!path) return;
        const row = el('p'); const button = el('button', 'text-button', attachment.title + ' ↗'); button.type = 'button';
        button.addEventListener('click', () => openImage(path, attachment.title, item.title, button)); row.append(button); extras.append(row);
      }); body.append(extras);
    }
    if (content) { const details = el('details', 'brief-details'); const summary = el('summary', '', summaryLabel || 'Open filming brief'); details.append(summary, content); body.append(details); }
    const actions = el('div', 'card-actions'); const copy = el('button', 'copy-brief', 'Copy brief'); copy.type = 'button'; copy.addEventListener('click', () => copyBrief(item, lane, copy)); actions.append(copy, el('span', 'idea-id', item.id)); body.append(actions);
    cardRecords.push({ node: card, lane, item, search: JSON.stringify(item).toLowerCase() });
    return card;
  }
  function commentCard(item) {
    const { card, body } = cardBase(item, 'comments');
    if (item.quote) body.append(el('blockquote', 'comment-quote', item.quote));
    const credit = [item.author, item.likes !== undefined && item.likes !== null && item.likes !== '' ? `${item.likes} likes at capture` : null].filter(Boolean).join(' · ');
    if (credit) body.append(el('p', 'comment-credit', credit));
    if (item.hook) { const hook = el('div', 'hook-box'); hook.append(el('span', 'field-label', 'Opening line'), el('p', 'hook', item.hook)); body.append(hook); }
    if (item.why) body.append(el('p', 'card-copy', item.why));
    const source = el('div', 'source-block'); source.append(el('span', 'field-label', 'Comment appeared on'), sourceLink(item.source_title || 'Original TikTok', item.source_url)); body.append(source);
    const content = el('div', 'brief-content'); detailsList(content, 'Video structure', item.beats, true); detailField(content, 'Production effort', item.effort); detailField(content, 'Production requirement', item.production); detailsList(content, 'Editor notes', item.notes, false);
    return finishCard(item, 'comments', card, body, content);
  }
  function aiCard(item) {
    const { card, body } = cardBase(item, 'ai');
    if (item.hook) body.append(el('span', 'field-label', 'Opening line'), el('p', 'hook', item.hook));
    if (item.change) body.append(el('span', 'field-label', 'The transformation'), el('p', 'card-copy', item.change));
    const source = el('div', 'source-block'); source.append(el('span', 'field-label', 'Original archive video'), sourceLink(item.original_title || 'Original TikTok', item.source_url));
    if (item.views) source.append(el('p', 'metric-line', `${item.views} displayed views${item.view_date ? ` · ${item.view_date}` : ''}`)); body.append(source);
    const content = el('div', 'brief-content'); detailsList(content, 'Sequence', item.beats, true); detailField(content, 'Raw footage required', item.raw_needed); detailField(content, 'Production effort', item.effort); detailField(content, 'Production requirement', item.production); detailsList(content, 'Fidelity & presentation', item.notes, false);
    return finishCard(item, 'ai', card, body, content);
  }
  // The original desk's topic card, kept for any angle that no hot topic absorbs.
  function topicCard(item) {
    const { card, body } = cardBase(item, 'topics');
    if (item.type) body.append(el('span', 'field-label', item.type));
    if (item.angle) body.append(el('p', 'card-copy', item.angle));
    if (item.hook) body.append(el('span', 'field-label', 'Opening line'), el('p', 'hook', item.hook));
    if (item.urgency) { const urgency = el('p', 'urgency-line'); urgency.append(el('strong', '', 'Timing'), document.createTextNode(item.urgency)); body.append(urgency); }
    if (item.publish_gate) { const gate = el('div', 'publish-gate'); gate.append(el('span', 'field-label', 'Before posting'), el('p', '', item.publish_gate)); body.append(gate); }
    const sources = el('div', 'source-block'); sources.append(el('span', 'field-label', 'Source trail')); list(item.sources).forEach((source) => { const row = el('p'); row.append(sourceLink(source.title, source.url)); sources.append(row); }); if (list(item.sources).length) body.append(sources);
    const content = el('div', 'brief-content'); detailField(content, 'Why this belongs', item.why); detailField(content, 'Recheck / expiry', item.expiry); detailField(content, 'Production requirement', item.production); detailsList(content, 'Research & filming notes', item.notes, false);
    return finishCard(item, 'topics', card, body, content);
  }

  // ---- What the other creators posted, and the hot topics (window.INTEL) ----
  const posts = Array.isArray(intel.posts) ? intel.posts : [];
  const postBySlug = Object.fromEntries(posts.map((post) => [post.slug, post]));
  const threads = Array.isArray(intel.threads) ? intel.threads : [];
  const threadByUrl = Object.fromEntries(threads.map((thread) => [thread.url, thread]));
  const commentById = Object.fromEntries(lanes.comments.map((comment) => [comment.id, comment]));
  const grids = intel.grids || {};
  const multipleOf = (value) => parseFloat(String(value || '').replace(/[^\d.]/g, '')) || 0;
  function multipleBadge(value) {
    if (!value) return null;
    const n = multipleOf(value);
    return badge(`${value} usual`, n >= 5 ? 'hot' : n >= 2 ? 'fact' : '');
  }
  function postCaptionLine(post) {
    return [post.views ? `${post.views} views` : '', post.multiple ? `${post.multiple} the creator's usual` : '', post.posted ? `posted ${post.posted}` : '', post.note || ''].filter(Boolean).join(' · ');
  }
  function evidenceThumb(post) {
    const path = safeImage(post.screenshot); if (!path) return null;
    const button = el('button', 'ev'); button.type = 'button';
    button.setAttribute('aria-label', `Enlarge ${post.creator}: ${post.caption || 'post'}`);
    const image = el('img'); image.src = path; image.alt = `${post.creator}: ${post.caption || ''}`; image.loading = 'lazy'; image.decoding = 'async';
    const cap = el('span', 'ev-cap'); cap.append(el('strong', '', post.views || '—'), document.createTextNode(`${post.creator}${post.multiple ? ` · ${post.multiple} usual` : ''}`));
    button.append(image, cap);
    button.addEventListener('click', () => openImage(path, `${post.creator}: ${post.caption || 'post'}`, postCaptionLine(post), button, post.url, 'Open on TikTok'));
    image.addEventListener('error', () => { button.hidden = true; });
    return button;
  }
  function evidenceStrip(items) {
    const strip = el('div', 'evidence-strip');
    items.forEach((post) => { const thumb = evidenceThumb(post); if (thumb) strip.append(thumb); });
    return strip.childElementCount ? strip : null;
  }
  function threadList(items, showSub) {
    const group = el('ul', 'thread-list');
    items.forEach((thread) => {
      const row = el('li');
      const score = el('span', 'score', thread.score ? Number(thread.score).toLocaleString('en-US') : '—'); score.append(el('small', '', `${thread.comments || 0} comments`));
      const text = el('span'); text.append(sourceLink(thread.title, thread.url));
      if (showSub) text.append(el('span', 'sub', `r/${thread.sub}${thread.created ? ` · ${thread.created}` : ''}`));
      row.append(score, text); group.append(row);
    });
    return group.childElementCount ? group : null;
  }
  function miniComment(comment) {
    const quote = el('p', 'mini-comment', comment.quote || comment.title);
    const credit = el('small'); credit.append(document.createTextNode([comment.author, comment.likes !== undefined && comment.likes !== '' ? `${comment.likes} likes` : ''].filter(Boolean).join(' · ') + ' · '));
    const link = el('a', '', 'see the card ↓'); link.href = `#${comment.id}`; credit.append(link); quote.append(credit);
    return quote;
  }
  function hotTopicBrief(item) {
    const lines = [item.title, 'Lane: Community topic (hot right now)', item.summary ? `What is happening: ${item.summary}` : '', ...item.posts.map((post) => `Post: ${post.creator} — ${post.caption || 'post'} — ${postCaptionLine(post)} — ${post.url}`), ...item.threads.map((thread) => `Reddit: r/${thread.sub} — ${thread.title} (${thread.score} points, ${thread.comments} comments) — ${thread.url}`), ...item.comments.map((comment) => `Comment on Elijah's post: “${comment.quote}”${comment.author ? ` — ${comment.author}` : ''}`), ...item.angles.flatMap((angle) => [`Angle: ${angle.title}`, angle.angle ? `  ${angle.angle}` : '', angle.hook ? `  Hook: ${angle.hook}` : '', angle.urgency ? `  Timing: ${angle.urgency}` : '', angle.publish_gate ? `  Before posting: ${angle.publish_gate}` : '', angle.why ? `  Why: ${angle.why}` : '', angle.expiry ? `  Recheck: ${angle.expiry}` : '', ...list(angle.notes).map((note) => `  Note: ${note}`), ...list(angle.sources).map((source) => `  ${source.title}: ${source.url}`)]), item.open ? `Still open: ${item.open}` : ''];
    return lines.filter(Boolean).join('\n');
  }
  function hotTopicCard(topic, index) {
    const topicPosts = list(topic.posts).map((slug) => postBySlug[slug]).filter(Boolean);
    const topicThreads = list(topic.threads).map((url) => threadByUrl[url]).filter(Boolean);
    const topicComments = list(topic.comments).map((id) => commentById[id]).filter(Boolean);
    const angles = lanes.topics.filter((angle) => list(topic.images).includes(angle.image));
    const lead = topicPosts.slice().sort((a, b) => multipleOf(b.multiple) - multipleOf(a.multiple))[0];
    const image = list(topic.images).map(safeImage).find(Boolean) || (lead && safeImage(lead.screenshot)) || null;
    const item = { id: `HOT${String(index + 1).padStart(2, '0')}`, title: topic.title, summary: topic.summary, open: topic.open, image, posts: topicPosts, threads: topicThreads, comments: topicComments, angles, caption_line: angles[0] ? angles[0].title : lead ? `${lead.creator}: ${lead.caption || ''} · ${postCaptionLine(lead)}` : topic.title, dialog_link: !angles[0] && lead ? lead.url : undefined, dialog_link_label: 'Open on TikTok' };
    const card = el('article', 'idea-card topics-card hot-card'); card.id = item.id;
    const preview = imagePreview(item, 'topics'); if (preview) card.append(preview);
    const body = el('div', 'card-body'); card.append(body);
    const chips = el('div', 'card-meta'); chips.append(badge(index === 0 ? 'Biggest this week' : 'Hot right now', 'hot'));
    angles.forEach((angle) => { if (angle.verified) chips.append(badge(angle.verified, /unverified|needs|check|pending|not verified|unavailable/i.test(angle.verified) ? 'check' : 'fact')); });
    body.append(chips, el('h3', '', topic.title));
    if (topic.summary) body.append(el('p', 'card-copy', topic.summary));
    const strip = evidenceStrip(topicPosts); if (strip) body.append(el('span', 'field-label', 'Posts getting views'), strip);
    const reddit = threadList(topicThreads, true); if (reddit) body.append(el('span', 'field-label', 'On Reddit'), reddit);
    if (topicComments.length) { body.append(el('span', 'field-label', "In Elijah's comments")); topicComments.forEach((comment) => body.append(miniComment(comment))); }
    angles.forEach((angle, angleIndex) => {
      // Each filming angle from the original desk folds under its title, so a topic with three angles stays short on a phone.
      const block = el('details', 'angle-block'); if (angleIndex === 0 && angles.length === 1) block.open = false;
      const summary = el('summary'); summary.append(el('span', 'field-label', angle.type ? `Angle from the desk · ${angle.type}` : 'Angle from the desk'), el('h4', '', angle.title)); block.append(summary);
      const inner = el('div', 'angle-inner'); block.append(inner);
      if (angle.angle) inner.append(el('p', 'card-copy', angle.angle));
      if (angle.hook) inner.append(el('span', 'field-label', 'Opening line'), el('p', 'hook', angle.hook));
      if (angle.urgency) { const urgency = el('p', 'urgency-line'); urgency.append(el('strong', '', 'Timing'), document.createTextNode(angle.urgency)); inner.append(urgency); }
      if (angle.publish_gate) { const gate = el('div', 'publish-gate'); gate.append(el('span', 'field-label', 'Before posting'), el('p', '', angle.publish_gate)); inner.append(gate); }
      if (list(angle.sources).length) { const sources = el('div', 'source-block'); sources.append(el('span', 'field-label', 'Source trail')); list(angle.sources).forEach((source) => { const row = el('p'); row.append(sourceLink(source.title, source.url)); sources.append(row); }); inner.append(sources); }
      if (Array.isArray(angle.attachments)) { const extras = el('div', 'source-block'); extras.append(el('span', 'field-label', 'Additional evidence')); angle.attachments.forEach((attachment) => { const path = safeImage(attachment.path); if (!path) return; const row = el('p'); const button = el('button', 'text-button', attachment.title + ' ↗'); button.type = 'button'; button.addEventListener('click', () => openImage(path, attachment.title, angle.title, button)); row.append(button); extras.append(row); }); inner.append(extras); }
      body.append(block);
    });
    const content = el('div', 'brief-content');
    angles.forEach((angle) => { if (angles.length > 1) content.append(el('span', 'field-label', angle.title)); detailField(content, 'Why this belongs', angle.why); detailField(content, 'Recheck / expiry', angle.expiry); detailField(content, 'Production requirement', angle.production); detailsList(content, 'Research & filming notes', angle.notes, false); });
    detailField(content, 'Still open', topic.open);
    if (topicPosts.length) { content.append(el('span', 'field-label', 'Evidence trail')); const trail = el('ul'); topicPosts.forEach((post) => { const row = el('li'); row.append(sourceLink(`${post.creator}: ${post.caption || 'post'} (${post.views}${post.posted ? `, ${post.posted}` : ''})`, post.url)); trail.append(row); }); content.append(trail); }
    const details = el('details', 'brief-details'); details.append(el('summary', '', angles.length ? 'Open filming brief' : 'Open notes & sources'), content); body.append(details);
    const actions = el('div', 'card-actions'); const copy = el('button', 'copy-brief', 'Copy brief'); copy.type = 'button'; copy.addEventListener('click', () => copyText(hotTopicBrief(item) + '\n\nCreative direction: minimal pop-ups, purposeful zooms, real product footage. No captions in V1/V2; clean captions allowed in V3.', copy, 'Brief')); actions.append(copy, el('span', 'idea-id', item.id)); body.append(actions);
    cardRecords.push({ node: card, lane: 'topics', item, search: JSON.stringify({ title: topic.title, summary: topic.summary, open: topic.open, posts: topicPosts.map((post) => [post.creator, post.handle, post.caption]), threads: topicThreads.map((thread) => thread.title), comments: topicComments.map((comment) => [comment.quote, comment.title]), angles: angles.map((angle) => [angle.title, angle.angle, angle.hook]) }).toLowerCase() });
    return card;
  }
  function postRow(post) {
    const row = el('div', 'post-row');
    const path = safeImage(post.screenshot);
    const thumb = el('button', 'post-thumb'); thumb.type = 'button'; thumb.setAttribute('aria-label', `Enlarge ${post.creator}: ${post.caption || 'post'}`);
    if (path) { const image = el('img'); image.src = path; image.alt = ''; image.loading = 'lazy'; image.decoding = 'async'; thumb.append(image); thumb.addEventListener('click', () => openImage(path, `${post.creator}: ${post.caption || 'post'}`, postCaptionLine(post), thumb, post.url, 'Open on TikTok')); image.addEventListener('error', () => { thumb.hidden = true; }); }
    else thumb.hidden = true;
    const text = el('div'); text.style.minWidth = '0';
    const top = el('div', 'post-top'); top.append(el('span', 'views', post.views || '—')); const mb = multipleBadge(post.multiple); if (mb) top.append(mb); if (post.posted) top.append(el('span', 'post-date', `posted ${post.posted}`)); text.append(top);
    if (post.caption) text.append(el('p', 'post-caption', post.caption));
    const source = el('div', 'source-block'); source.append(sourceLink('Open on TikTok', post.url)); text.append(source);
    if (post.note) text.append(el('p', 'post-note', post.note));
    row.append(thumb, text);
    return row;
  }
  function creatorCard(group) {
    const first = group.posts[0];
    const best = group.posts.slice().sort((a, b) => multipleOf(b.multiple) - multipleOf(a.multiple))[0];
    const handle = group.handle;
    const item = { id: `@${handle}`, title: group.handle === 'fragrances' ? `${first.creator} (for comparison)` : first.creator, image: safeImage(grids[handle]) || safeImage(first.screenshot), caption_line: safeImage(grids[handle]) ? `@${handle} profile grid on October 25${first.followers ? ` · ${first.followers} followers` : ''}` : `${first.creator}: ${first.caption || ''}`, dialog_link: `https://www.tiktok.com/@${handle}`, dialog_link_label: 'Open the profile', handle, followers: first.followers, median: first.median, posts: group.posts };
    const card = el('article', 'idea-card creators-card'); card.id = item.id;
    const preview = imagePreview(item, 'creators'); if (preview) card.append(preview);
    const body = el('div', 'card-body'); card.append(body);
    const chips = el('div', 'card-meta'); chips.append(badge(`@${handle}`)); if (first.followers) chips.append(badge(`${first.followers} followers`, 'priority')); if (best && multipleOf(best.multiple) >= 5) chips.append(badge(`best post ${best.multiple} usual`, 'hot'));
    body.append(chips, el('h3', '', item.title));
    if (first.median) body.append(el('p', 'creator-usual', `A usual post for this account is about ${first.median} views (median of their unpinned posts since Oct 4). The multiples below are measured against that.`));
    const rows = el('div', 'post-list'); group.posts.slice().sort((a, b) => multipleOf(b.multiple) - multipleOf(a.multiple)).forEach((post) => rows.append(postRow(post))); body.append(el('span', 'field-label', group.posts.length === 1 ? 'The post' : `${group.posts.length} posts worth knowing`), rows);
    const actions = el('div', 'card-actions'); const profile = el('a', 'copy-brief', 'Open profile ↗'); profile.href = `https://www.tiktok.com/@${handle}`; profile.target = '_blank'; profile.rel = 'noopener noreferrer'; actions.append(profile, el('span', 'idea-id', `@${handle}`)); body.append(actions);
    cardRecords.push({ node: card, lane: 'creators', item, search: JSON.stringify({ creator: first.creator, handle, posts: group.posts.map((post) => [post.caption, post.views, post.posted, post.note]) }).toLowerCase() });
    return card;
  }
  function bankCard(bank) {
    const item = { id: 'BANK', title: `${bank.length} more fragrance accounts, by size`, bank };
    const card = el('article', 'idea-card creators-card bank-card'); card.id = item.id;
    const body = el('div', 'card-body'); card.append(body);
    const chips = el('div', 'card-meta'); chips.append(badge('Creator bank', 'priority')); body.append(chips, el('h3', '', item.title));
    body.append(el('p', 'card-copy', 'Profile grids for the wider field, captured for the research bank. Tap one to see what that account was posting; the follower count is from the profile at capture.'));
    const strip = el('div', 'grid-strip');
    bank.forEach((creator) => {
      const path = safeImage(creator.grid); if (!path) return;
      const button = el('button', 'ev'); button.type = 'button'; button.setAttribute('aria-label', `Enlarge ${creator.creator} profile grid`);
      const image = el('img'); image.src = path; image.alt = ''; image.loading = 'lazy'; image.decoding = 'async';
      const cap = el('span', 'ev-cap'); cap.append(el('strong', '', creator.creator), document.createTextNode(`@${creator.handle}${creator.followers ? ` · ${creator.followers}` : ''}`));
      button.append(image, cap); button.addEventListener('click', () => openImage(path, `${creator.creator} (@${creator.handle})`, `Profile grid at capture${creator.followers ? ` · ${creator.followers} followers` : ''}`, button, creator.url, 'Open the profile')); image.addEventListener('error', () => { button.hidden = true; });
      strip.append(button);
    });
    body.append(strip);
    const actions = el('div', 'card-actions'); actions.append(el('span', 'idea-id', 'BANK')); body.append(actions);
    cardRecords.push({ node: card, lane: 'creators', item, search: JSON.stringify(bank.map((creator) => [creator.creator, creator.handle])).toLowerCase() });
    return card;
  }
  function redditCard(sub, info) {
    const subThreads = threads.filter((thread) => thread.sub === sub).slice(0, 10);
    const item = { id: `r/${sub}`, title: `r/${sub}: top this week`, image: safeImage(info.screenshot), caption_line: `r/${sub} top posts of the week, captured October 25`, dialog_link: info.url, dialog_link_label: 'Open on Reddit', threads: subThreads };
    const card = el('article', 'idea-card reddit-card'); card.id = item.id;
    const preview = imagePreview(item, 'reddit'); if (preview) card.append(preview);
    const body = el('div', 'card-body'); card.append(body);
    const chips = el('div', 'card-meta'); chips.append(badge(`${info.n || subThreads.length} threads captured`)); body.append(chips, el('h3', '', item.title));
    const group = threadList(subThreads, false); if (group) body.append(el('span', 'field-label', 'Most upvoted, past 7 days'), group);
    const actions = el('div', 'card-actions'); const open = el('a', 'copy-brief', `Open r/${sub} top ↗`); open.href = safeLink(info.url) || '#'; open.target = '_blank'; open.rel = 'noopener noreferrer'; actions.append(open, el('span', 'idea-id', `r/${sub}`)); body.append(actions);
    cardRecords.push({ node: card, lane: 'reddit', item, search: JSON.stringify(subThreads.map((thread) => thread.title)).toLowerCase() });
    return card;
  }

  function resetForAnchor() {
    $('#idea-search').value = ''; $('#lane-filter').value = 'all'; applyFilters();
  }
  function applyFilters() {
    const term = $('#idea-search').value.trim().toLowerCase(); const lane = $('#lane-filter').value;
    let visible = 0;
    cardRecords.forEach((record) => {
      const show = (!term || record.search.includes(term)) && (lane === 'all' || lane === record.lane);
      record.node.hidden = !show; if (show) visible++;
    });
    document.querySelectorAll('.lane-section').forEach((section) => { const laneKey = section.dataset.lane; section.hidden = lane !== 'all' && lane !== laneKey; const hasItems = cardRecords.some((record) => record.lane === laneKey && !record.node.hidden); section.querySelector('.lane-empty').hidden = hasItems; });
    $('#result-count').textContent = `${visible} of ${cardRecords.length} cards`;
    $('#clear-filters').hidden = !term && lane === 'all'; $('#empty-search').hidden = visible > 0;
  }

  if (meta.title) document.title = meta.title;
  $('#research-stamp').textContent = meta.research_date ? `Research snapshot · ${meta.research_date}` : 'Working research snapshot';
  $('#source-note').textContent = meta.source_note || 'Open each source before filming. Public screenshots establish what was visible at capture; proposed concepts are editorial ideas.';
  $('#intel-note').textContent = (intel.meta && intel.meta.method) || '';
  $('#api-note').textContent = meta.api_status || '';

  lanes.comments.forEach((item) => $('#comments-grid').append(commentCard(item)));
  lanes.ai.forEach((item) => $('#ai-grid').append(aiCard(item)));
  const hotTopics = Array.isArray(intel.topics) ? intel.topics : [];
  const absorbed = new Set();
  hotTopics.forEach((topic, index) => { lanes.topics.forEach((angle) => { if (list(topic.images).includes(angle.image)) absorbed.add(angle.id); }); $('#topics-grid').append(hotTopicCard(topic, index)); });
  lanes.topics.filter((angle) => !absorbed.has(angle.id)).forEach((item) => $('#topics-grid').append(topicCard(item)));
  const groups = []; const byHandle = {};
  posts.forEach((post) => { if (!byHandle[post.handle]) { byHandle[post.handle] = { handle: post.handle, posts: [] }; groups.push(byHandle[post.handle]); } byHandle[post.handle].posts.push(post); });
  groups.sort((a, b) => Math.max(...b.posts.map((post) => multipleOf(post.multiple))) - Math.max(...a.posts.map((post) => multipleOf(post.multiple))));
  groups.forEach((group) => $('#creators-grid').append(creatorCard(group)));
  const bank = (Array.isArray(intel.bank) ? intel.bank : []).filter((creator) => !byHandle[creator.handle]);
  if (bank.length) $('#creators-grid').append(bankCard(bank));
  Object.entries(intel.community || {}).forEach(([sub, info]) => $('#reddit-grid').append(redditCard(sub, info)));
  ['comments', 'ai', 'topics', 'creators', 'reddit'].forEach((lane) => { const count = cardRecords.filter((record) => record.lane === lane).length; const node = $(`#${lane}-count`); if (node) node.textContent = String(count).padStart(2, '0'); });

  $('#idea-search').addEventListener('input', applyFilters); $('#lane-filter').addEventListener('change', applyFilters); $('#clear-filters').addEventListener('click', resetForAnchor);
  $('#print-page').addEventListener('click', () => { document.querySelectorAll('details').forEach((details) => { details.dataset.wasOpen = String(details.open); details.open = true; }); window.print(); });
  window.addEventListener('afterprint', () => { document.querySelectorAll('details').forEach((details) => { details.open = details.dataset.wasOpen === 'true'; delete details.dataset.wasOpen; }); });
  $('#close-image').addEventListener('click', () => dialog.close());
  dialog.addEventListener('click', (event) => { if (event.target === dialog) { const bounds = dialog.getBoundingClientRect(); if (event.clientX < bounds.left || event.clientX > bounds.right || event.clientY < bounds.top || event.clientY > bounds.bottom) dialog.close(); } });
  dialog.addEventListener('close', () => { if (lastImageTrigger) lastImageTrigger.focus(); });
  applyFilters();
  if (window.location.hash) window.requestAnimationFrame(() => { const target = document.getElementById(decodeURIComponent(window.location.hash.slice(1))); if (target) target.scrollIntoView(); });
})();
