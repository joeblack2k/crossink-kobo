(() => {
  'use strict';

  const SECTION_ORDER = [
    ['reading', 'Reading defaults', 'Typography, layout and EPUB rendering'],
    ['display', 'Display & interface', 'Screen, frontlight and interface'],
    ['library', 'Library & files', 'Library and file browser behaviour'],
    ['controls', 'Controls', 'Touch and physical-button actions'],
    ['power', 'Power', 'Sleep and resume behaviour'],
    ['network', 'Network & servers', 'Wi-Fi, OPDS, sync and transfer services'],
    ['system', 'System', 'Device identity, clock and statistics'],
  ];
  const SECTION_BY_KEY = {
    fontFamily: 'reading', fontSize: 'reading', sdFontSizeRange: 'reading', lineHeightPercent: 'reading',
    screenMargin: 'reading', paragraphAlignment: 'reading', embeddedStyle: 'reading', hyphenationEnabled: 'reading',
    textAntiAliasing: 'reading', readerDarkMode: 'reading', imageRendering: 'reading', extraParagraphSpacing: 'reading',
    forceParagraphIndents: 'reading', bionicReadingEnabled: 'reading', guideReadingEnabled: 'reading', orientation: 'reading',
    frontlightBrightness: 'display', koboUiScalePercent: 'display', uiTheme: 'display', refreshFrequency: 'display',
    hideBatteryPercentage: 'display', hideClock: 'display', statusBarChapterPageCount: 'display', stablePageNumbers: 'display',
    statusBarBookProgressPercentage: 'display', statusBarProgressBar: 'display', statusBarProgressBarThickness: 'display',
    statusBarTitle: 'display', statusBarTimeLeft: 'display', statusBarBattery: 'display', xtcStatusBarMode: 'display',
    recentBooksView: 'library', showHiddenFiles: 'library', hideFileExtension: 'library', fileBrowserDisplay: 'library',
    removeReadBooksFromRecents: 'library', moveFinishedToReadFolder: 'library', trackReadingStats: 'library',
    sideButtonLayout: 'controls', sideButtonOrientationAware: 'controls', sideButtonLongPress: 'controls',
    frontButtonOrientationAware: 'controls', longPressButtonBehavior: 'controls', shortPwrBtn: 'controls', longPwrBtn: 'controls',
    longPressMenuAction: 'controls', longPressBackAction: 'controls', pwrBtnFootnoteBack: 'controls',
    sleepTimeoutMinutes: 'power', sleepScreen: 'power', sleepScreenCoverMode: 'power', sleepScreenCoverFilter: 'power',
    quickResumeSleepScreen: 'power', deviceName: 'system', clockUtcOffsetQ: 'system', clockFormat: 'system',
    clockHasBeenSynced: 'system', autoBackupStats: 'system', readingIdleTimeThresholdUnits: 'system',
  };

  const state = { settings: [], original: new Map(), activeSection: 'reading', activeView: 'dashboard', status: null };
  const $ = (id) => document.getElementById(id);

  function escapeHtml(value) {
    return String(value ?? '').replace(/[&<>'"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', "'": '&#039;', '"': '&quot;' }[c]));
  }
  async function request(url, options = {}) {
    const response = await fetch(url, options);
    const type = response.headers.get('content-type') || '';
    const body = type.includes('application/json') ? await response.json() : await response.text();
    if (!response.ok) throw new Error(typeof body === 'string' ? body : (body.error || 'Request failed'));
    return body;
  }
  function message(text, error = false) {
    const el = $('portal-message');
    el.textContent = text; el.className = `portal-message visible ${error ? 'error' : 'ok'}`;
    window.clearTimeout(message.timer); message.timer = window.setTimeout(() => { el.className = 'portal-message'; }, 5200);
  }
  function settingSection(setting) { return setting.section || SECTION_BY_KEY[setting.key] || 'system'; }
  function currentValue(setting) {
    const control = document.querySelector(`[data-setting-key="${CSS.escape(setting.key)}"]`);
    if (!control) return undefined;
    if (setting.type === 'toggle') return control.checked ? 1 : 0;
    if (setting.secret) return control.value;
    return setting.type === 'value' || setting.type === 'enum' ? Number(control.value) : control.value;
  }
  function changedKeys(section) {
    return state.settings.filter((setting) => settingSection(setting) === section).filter((setting) => {
      const value = currentValue(setting);
      return value !== undefined && value !== state.original.get(setting.key) && (!setting.secret || value.length > 0);
    });
  }
  function settingControl(setting) {
    const common = `data-setting-key="${escapeHtml(setting.key)}" data-section="${settingSection(setting)}"`;
    if (setting.type === 'toggle') return `<label class="toggle-control"><input type="checkbox" ${common} ${setting.value ? 'checked' : ''}><span>${setting.value ? 'On' : 'Off'}</span></label>`;
    if (setting.type === 'enum') return `<select ${common}>${(setting.options || []).map((option, index) => `<option value="${index}" ${Number(setting.value) === index ? 'selected' : ''}>${escapeHtml(option)}</option>`).join('')}</select>`;
    if (setting.type === 'value') return `<input ${common} type="number" value="${Number(setting.value)}" min="${Number(setting.min)}" max="${Number(setting.max)}" step="${Number(setting.step) || 1}">`;
    const type = setting.secret ? 'password' : 'text';
    const value = setting.secret ? '' : escapeHtml(setting.value || '');
    const placeholder = setting.secret && setting.hasValue ? 'Configured — leave blank to keep' : '';
    return `<input ${common} type="${type}" value="${value}" placeholder="${placeholder}">`;
  }
  function renderSettings() {
    const nav = $('settings-nav'); const container = $('settings-sections');
    nav.replaceChildren(); container.replaceChildren();
    for (const [section, title, description] of SECTION_ORDER) {
      const settings = state.settings.filter((setting) => settingSection(setting) === section);
      const navButton = document.createElement('button'); navButton.type = 'button'; navButton.textContent = title;
      navButton.className = section === state.activeSection ? 'active' : '';
      navButton.addEventListener('click', () => { state.activeSection = section; renderSettings(); }); nav.appendChild(navButton);
      const view = document.createElement('section'); view.className = `settings-section ${section === state.activeSection ? 'active' : ''}`; view.dataset.section = section;
      view.innerHTML = `<article class="settings-card"><h3>${title}</h3><p>${description}</p><div class="setting-list">${settings.map((setting) => `<div class="setting-row" data-setting-row="${escapeHtml(setting.key)}"><div><label for="setting-${escapeHtml(setting.key)}">${escapeHtml(setting.name)}</label><small>${escapeHtml(setting.description || effectLabel(setting.effect))}</small></div><div>${settingControl(setting)}</div></div>`).join('') || '<p>No configurable settings in this section.</p>'}</div></article>` +
        `<div class="section-save" data-save-bar="${section}" hidden><span data-dirty-count="${section}">0 changes</span><span><button class="button button-ghost" data-discard="${section}">Discard</button> <button class="button button-primary" data-save="${section}">Save changes</button></span></div>`;
      container.appendChild(view);
    }
    renderNetworkServices();
    container.querySelectorAll('[data-setting-key]').forEach((control) => {
      control.addEventListener('input', settingsInputChanged);
      control.addEventListener('change', settingsInputChanged);
    });
    container.querySelectorAll('[data-save]').forEach((button) => button.addEventListener('click', () => saveSection(button.dataset.save)));
    container.querySelectorAll('[data-discard]').forEach((button) => button.addEventListener('click', () => discardSection(button.dataset.discard)));
  }
  function effectLabel(effect) { return effect === 'reboot' ? 'Applies after reboot' : effect === 'reopen-reader' ? 'Applies when the reader is reopened' : 'Applies immediately'; }
  function settingsInputChanged(event) {
    const control = event.target.closest('[data-setting-key]'); if (!control) return;
    if (control.type === 'checkbox') control.nextElementSibling.textContent = control.checked ? 'On' : 'Off';
    updateSectionDirty(control.dataset.section);
  }
  function updateSectionDirty(section) {
    const changes = changedKeys(section); const bar = document.querySelector(`[data-save-bar="${section}"]`);
    if (bar) { bar.hidden = changes.length === 0; bar.querySelector('[data-dirty-count]').textContent = `${changes.length} change${changes.length === 1 ? '' : 's'}`; }
  }
  function discardSection(section) {
    state.settings.filter((setting) => settingSection(setting) === section).forEach((setting) => {
      const control = document.querySelector(`[data-setting-key="${CSS.escape(setting.key)}"]`); if (!control) return;
      const value = state.original.get(setting.key);
      if (setting.type === 'toggle') { control.checked = Boolean(value); control.nextElementSibling.textContent = value ? 'On' : 'Off'; }
      else if (!setting.secret) control.value = value;
      else control.value = '';
    });
    updateSectionDirty(section);
  }
  async function saveSection(section) {
    const changed = changedKeys(section); if (!changed.length) return;
    const payload = {};
    for (const setting of changed) payload[setting.key] = currentValue(setting);
    try {
      const result = await request('/api/settings/apply', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(payload) });
      changed.forEach((setting) => { state.original.set(setting.key, payload[setting.key]); if (setting.secret) document.querySelector(`[data-setting-key="${CSS.escape(setting.key)}"]`).value = ''; });
      const suffix = result.rebootRequired ? ' Reboot required.' : result.reopenReaderRequired ? ' Reopen the reader to apply layout changes.' : '';
      message(`Saved ${result.applied.length} setting${result.applied.length === 1 ? '' : 's'}.${suffix}`); renderSettings(); await loadDashboard();
    } catch (error) { message(`Settings were not saved: ${error.message}`, true); }
  }
  async function loadSettings() {
    const result = await request('/api/settings'); state.settings = Array.isArray(result) ? result : result.settings;
    state.original.clear(); state.settings.forEach((setting) => state.original.set(setting.key, setting.value)); renderSettings();
  }
  function networkMarkup() { return `<section class="settings-section ${state.activeSection === 'network' ? 'active' : ''}" data-section="network"><article class="settings-card"><h3>Network & servers</h3><p>Configure local access and reading services.</p><div id="network-services"><div class="loading-block">Loading services…</div></div></article></section>`; }
  async function renderNetworkServices() {
    const networkSection = document.querySelector('.settings-section[data-section="network"]'); if (!networkSection) return;
    networkSection.innerHTML = `<article class="settings-card"><h3>Network & servers</h3><p>Configure local access and reading services.</p><div id="network-services"><div class="loading-block">Loading services…</div></div></article>`;
    const target = $('network-services');
    try {
      const [services, wifi, opds] = await Promise.all([request('/api/network/services'), request('/api/wifi'), request('/api/opds')]);
      target.innerHTML = `<h4>Web transfer & WebDAV</h4><div class="network-note"><strong>${services.webTransferEnabled ? 'Enabled' : 'Disabled'}</strong> · ${escapeHtml(services.webUrl || 'Not available')}<br>WebDAV: ${escapeHtml(services.webdavUrl || 'Not available')}<br>Calibre Wireless: ${escapeHtml(services.calibre || 'Available when Wi-Fi is connected')}</div>` +
        `<h4>Wi-Fi networks</h4>${wifi.map((entry) => wifiCard(entry)).join('') || '<p>No saved Wi-Fi networks.</p>'}<button class="button button-ghost" id="add-wifi">Add Wi-Fi network</button>` +
        `<h4>OPDS catalogs</h4>${opds.map((entry) => opdsCard(entry)).join('') || '<p>No OPDS servers configured.</p>'}<button class="button button-ghost" id="add-opds">Add OPDS server</button>` +
        `<h4>KOReader Sync</h4><div class="server-card" id="koreader-card">${koreaderCard(services.koreader || {})}</div>`;
      bindNetworkActions();
    } catch (error) { target.innerHTML = `<div class="network-note">Could not load network services: ${escapeHtml(error.message)}</div>`; }
  }
  function wifiCard(entry = {}) { const id = entry.id ?? 'new'; return `<div class="server-card" data-wifi="${escapeHtml(id)}"><div class="server-grid"><label>SSID<input data-field="ssid" value="${escapeHtml(entry.ssid || '')}"></label><label>Password<input data-field="password" type="password" placeholder="${entry.hasPassword ? 'Configured — leave blank to keep' : ''}"></label></div><div class="server-actions"><button class="button" data-wifi-save="${escapeHtml(id)}">Save</button>${entry.id !== undefined ? `<button class="button danger" data-wifi-delete="${escapeHtml(id)}">Delete</button>` : ''}</div></div>`; }
  function opdsCard(entry = {}) { const id = entry.id ?? 'new'; return `<div class="server-card ${entry.primary ? 'primary' : ''}" data-opds="${escapeHtml(id)}"><h4>${entry.primary ? 'Primary catalog' : 'Catalog'}</h4><div class="server-grid"><label>Name<input data-field="name" value="${escapeHtml(entry.name || '')}"></label><label>URL<input data-field="url" value="${escapeHtml(entry.url || '')}"></label><label>Username<input data-field="username" value="${escapeHtml(entry.username || '')}"></label><label>Password<input data-field="password" type="password" placeholder="${entry.hasPassword ? 'Configured — leave blank to keep' : ''}"></label><label>Filename<select data-field="filenameFormat"><option value="author_title" ${(entry.filenameFormat || 'author_title') === 'author_title' ? 'selected' : ''}>Author - Title</option><option value="title_author" ${entry.filenameFormat === 'title_author' ? 'selected' : ''}>Title - Author</option></select></label><label class="toggle-control"><input data-field="syncAllBooks" type="checkbox" ${entry.syncAllBooks ? 'checked' : ''}><span>Sync all books</span></label></div><div class="server-actions"><button class="button" data-opds-save="${escapeHtml(id)}">Save</button><button class="button" data-opds-test="${escapeHtml(id)}">Test connection</button>${entry.id !== undefined && !entry.primary ? `<button class="button" data-opds-primary="${escapeHtml(id)}">Make primary</button>` : ''}${entry.id !== undefined ? `<button class="button danger" data-opds-delete="${escapeHtml(id)}">Delete</button>` : ''}</div></div>`; }
  function koreaderCard(entry) { return `<div class="server-grid"><label>Server URL<input data-ko="serverUrl" value="${escapeHtml(entry.serverUrl || '')}" placeholder="Default KOReader sync server"></label><label>Username<input data-ko="username" value="${escapeHtml(entry.username || '')}"></label><label>Password<input data-ko="password" type="password" placeholder="${entry.hasPassword ? 'Configured — leave blank to keep' : ''}"></label><label>Document matching<select data-ko="matchMethod"><option value="0" ${Number(entry.matchMethod || 0) === 0 ? 'selected' : ''}>Filename</option><option value="1" ${Number(entry.matchMethod) === 1 ? 'selected' : ''}>Binary</option></select></label></div><div class="server-actions"><button class="button" data-ko-save>Save</button><button class="button" data-ko-test>Test connection</button></div>`; }
  function cardData(card, selector) { const result = {}; card.querySelectorAll(selector).forEach((field) => { result[field.dataset.field || field.dataset.ko] = field.type === 'checkbox' ? field.checked : field.value.trim(); }); return result; }
  function bindNetworkActions() {
    $('add-wifi')?.addEventListener('click', () => { $('add-wifi').insertAdjacentHTML('beforebegin', wifiCard()); bindNetworkActions(); });
    $('add-opds')?.addEventListener('click', () => { $('add-opds').insertAdjacentHTML('beforebegin', opdsCard()); bindNetworkActions(); });
    document.querySelectorAll('[data-wifi-save]').forEach((button) => button.addEventListener('click', async () => { const card = button.closest('[data-wifi]'); const id = card.dataset.wifi; try { await request('/api/wifi', { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({...cardData(card, '[data-field]'), ...(id === 'new' ? {} : {id})}) }); message('Wi-Fi network saved.'); renderNetworkServices(); } catch (e) { message(e.message,true); } }));
    document.querySelectorAll('[data-wifi-delete]').forEach((button) => button.addEventListener('click', async () => { if (!confirm('Delete this Wi-Fi network?')) return; try { await request('/api/wifi/delete',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({id:button.dataset.wifiDelete})}); message('Wi-Fi network deleted.'); renderNetworkServices(); } catch(e){message(e.message,true);} }));
    document.querySelectorAll('[data-opds-save]').forEach((button) => button.addEventListener('click', async () => { const card=button.closest('[data-opds]'); const id=card.dataset.opds; const data=cardData(card,'[data-field]'); if (!data.name || !data.url) return message('OPDS name and URL are required.',true); try { await request('/api/opds',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({...data,...(id==='new'?{}:{id})})}); message('OPDS catalog saved.'); renderNetworkServices(); loadDashboard(); }catch(e){message(e.message,true);} }));
    document.querySelectorAll('[data-opds-test]').forEach((button) => button.addEventListener('click', async () => { const card=button.closest('[data-opds]'); try { const data=cardData(card,'[data-field]'); const result=await request('/api/opds/test',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)}); message(result.message || 'OPDS connection succeeded.'); }catch(e){message(`OPDS test failed: ${e.message}`,true);} }));
    document.querySelectorAll('[data-opds-primary]').forEach((button) => button.addEventListener('click', async () => { try { await request('/api/opds/primary',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({id:button.dataset.opdsPrimary})}); message('Primary OPDS catalog updated.'); renderNetworkServices(); loadDashboard(); }catch(e){message(e.message,true);} }));
    document.querySelectorAll('[data-opds-delete]').forEach((button) => button.addEventListener('click', async () => { if (!confirm('Delete this OPDS catalog?')) return; try { await request('/api/opds/delete',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({id:button.dataset.opdsDelete})}); message('OPDS catalog deleted.'); renderNetworkServices(); loadDashboard(); }catch(e){message(e.message,true);} }));
    document.querySelector('[data-ko-save]')?.addEventListener('click', () => saveKoReader(false)); document.querySelector('[data-ko-test]')?.addEventListener('click', () => saveKoReader(true));
  }
  async function saveKoReader(test) { const card=$('koreader-card'); try { const result=await request(test?'/api/koreader/test':'/api/koreader',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(cardData(card,'[data-ko]'))}); message(result.message || (test?'KOReader connection succeeded.':'KOReader settings saved.')); if(!test){renderNetworkServices();loadDashboard();} }catch(e){message(`KOReader: ${e.message}`,true);} }
  async function loadDashboard() { try { const [status, services, opds] = await Promise.all([request('/api/status'), request('/api/network/services'), request('/api/opds')]); state.status=status; $('portal-version').textContent=status.version || 'CrossInk'; $('status-device').textContent=status.device || 'Kobo Glo HD'; $('status-version').textContent=status.version || '—'; $('status-battery').textContent=status.battery ? `${status.battery}%` : '—'; $('status-frontlight').textContent=status.frontlight !== undefined ? `Frontlight ${status.frontlight}%` : 'Frontlight —'; $('status-wifi').textContent=status.wifi || 'Offline'; $('status-ip').textContent=status.ip || 'No IP address'; $('status-storage').textContent=status.storageFree || '—'; $('service-web').textContent=services.webTransferEnabled ? (services.webUrl || 'Enabled') : 'Disabled'; $('service-dav').textContent=services.webdavUrl || 'Unavailable'; $('service-calibre').textContent=services.calibre || 'Available on Wi-Fi'; $('service-opds').textContent=(opds.find((entry)=>entry.primary)||{}).name || 'Not configured'; $('service-koreader').textContent=services.koreader?.configured ? 'Configured' : 'Not configured'; const pill=$('connection-pill'); pill.textContent=status.wifi || 'Local connection'; pill.className='connection-pill online'; } catch(error) { $('connection-pill').textContent='Connection error'; $('connection-pill').className='connection-pill error'; } }
  async function loadFiles() { const el=$('file-list'); try { const files=await request('/api/files?path=/Books'); el.innerHTML=files.length?files.map((file)=>`<div class="file-row"><strong>${escapeHtml(file.name)}</strong><small>${Number(file.size||0).toLocaleString()} bytes</small></div>`).join(''):'<p>No books uploaded yet.</p>'; }catch(e){el.textContent=`Could not load books: ${e.message}`;} }
  async function uploadBooks() { const files=[...$('book-upload').files]; if(!files.length)return; const button=$('book-upload-submit'); button.disabled=true; try { for(const file of files){const data=new FormData();data.append('file',file);await request('/upload?path=/Books',{method:'POST',body:data});} message(`Uploaded ${files.length} book${files.length===1?'':'s'}.`);$('book-upload').value='';$('book-upload-hint').textContent='Upload complete.';await loadFiles();}catch(e){message(`Upload failed: ${e.message}`,true);}finally{button.disabled=false;} }
  async function loadFonts() { const el=$('font-list'); try { const data=await request('/api/fonts'); const families=data.families||[];el.innerHTML=families.length?families.map((font)=>`<div class="file-row"><strong>${escapeHtml(font.name)}</strong><small>${escapeHtml((font.sizes||[]).join(', '))} pt</small></div>`).join(''):'<p>No custom fonts installed.</p>';}catch(e){el.textContent='Font service unavailable on this build.';} }
  function setView(view) { state.activeView=view; document.querySelectorAll('.portal-view').forEach((node)=>node.classList.toggle('active',node.id===`view-${view}`)); document.querySelectorAll('.portal-nav-item').forEach((node)=>node.classList.toggle('active',node.dataset.view===view)); $('portal-title').textContent={dashboard:'Dashboard',files:'Library & files',settings:'Settings',fonts:'Fonts'}[view]; document.querySelector('.portal-shell').classList.remove('nav-open'); if(view==='settings')loadSettings().catch((e)=>message(e.message,true)); if(view==='files')loadFiles(); if(view==='fonts')loadFonts(); }
  function init() { document.querySelectorAll('.portal-nav-item').forEach((button)=>button.addEventListener('click',()=>setView(button.dataset.view))); document.querySelectorAll('[data-goto]').forEach((button)=>button.addEventListener('click',()=>setView(button.dataset.goto))); $('nav-toggle').addEventListener('click',()=>document.querySelector('.portal-shell').classList.toggle('nav-open')); $('book-upload').addEventListener('change',()=>{const count=$('book-upload').files.length;$('book-upload-submit').disabled=!count;$('book-upload-hint').textContent=count?`${count} EPUB file${count===1?'':'s'} selected.`:'Only normal EPUB files are accepted.';}); $('book-upload-submit').addEventListener('click',uploadBooks); $('files-refresh').addEventListener('click',loadFiles); $('settings-search').addEventListener('input',(event)=>{const term=event.target.value.toLowerCase();document.querySelectorAll('.setting-row').forEach((row)=>{row.hidden=term&&!row.textContent.toLowerCase().includes(term);});}); loadDashboard(); }
  window.addEventListener('DOMContentLoaded',init);
})();
