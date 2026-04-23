// ============================================================
//  FitForge — app.js
//  IMPORTANT: Replace the two constants below with your own
//  Supabase project URL and anon/public key before running.
// ============================================================

const SUPABASE_URL = 'https://YOUR_PROJECT_ID.supabase.co';
const SUPABASE_ANON_KEY = 'YOUR_ANON_KEY_HERE';

// ── Init Supabase client ──────────────────────────────────────
const { createClient } = supabase;
const sb = createClient(SUPABASE_URL, SUPABASE_ANON_KEY);

// ── In-memory state ──────────────────────────────────────────
let currentUser = null;
let workouts    = [];
let meals       = [];
let goals       = [];
let schedules   = [];
let metrics     = { weight: null, height: null, fat: null, muscle: null };

// ============================================================
//  AUTH
// ============================================================

/** Switch between Sign-In / Sign-Up tabs */
function switchTab(tab) {
  document.querySelectorAll('.auth-tab').forEach(t => t.classList.remove('active'));
  document.querySelectorAll('.auth-form').forEach(f => f.classList.remove('active'));
  document.querySelector(`[onclick="switchTab('${tab}')"]`).classList.add('active');
  document.getElementById(`${tab}-form`).classList.add('active');
}

/** Sign-In */
async function signIn() {
  const email    = document.getElementById('signin-email').value.trim();
  const password = document.getElementById('signin-password').value;
  const msg      = document.getElementById('signin-msg');

  if (!email || !password) { setMsg(msg, 'Please fill in all fields.'); return; }

  setMsg(msg, 'Signing in…');
  const { data, error } = await sb.auth.signInWithPassword({ email, password });

  if (error) { setMsg(msg, error.message); return; }

  currentUser = data.user;
  enterApp();
}

/** Sign-Up */
async function signUp() {
  const name     = document.getElementById('signup-name').value.trim();
  const email    = document.getElementById('signup-email').value.trim();
  const password = document.getElementById('signup-password').value;
  const msg      = document.getElementById('signup-msg');

  if (!name || !email || !password) { setMsg(msg, 'Please fill in all fields.'); return; }
  if (password.length < 6)          { setMsg(msg, 'Password must be at least 6 characters.'); return; }

  setMsg(msg, 'Creating account…');
  const { data, error } = await sb.auth.signUp({
    email,
    password,
    options: { data: { full_name: name } }
  });

  if (error) { setMsg(msg, error.message); return; }

  // Supabase may require email confirmation
  if (data.user && data.user.identities && data.user.identities.length === 0) {
    setMsg(msg, 'Email already registered. Please sign in.', true);
    return;
  }

  setMsg(msg, '✅ Account created! Check your email to confirm, then sign in.', true);
}

/** Sign-Out */
async function signOut() {
  await sb.auth.signOut();
  currentUser = null;
  workouts = []; meals = []; goals = []; schedules = [];
  metrics = { weight: null, height: null, fat: null, muscle: null };
  document.getElementById('app').classList.add('hidden');
  const overlay = document.getElementById('auth-overlay');
  overlay.classList.add('active');
  overlay.classList.remove('hidden');
  document.getElementById('signin-email').value = '';
  document.getElementById('signin-password').value = '';
  document.getElementById('signin-msg').textContent = '';
}

/** Helper to set auth message */
function setMsg(el, text, success = false) {
  el.textContent = text;
  el.className = 'auth-msg' + (success ? ' success' : '');
}

/** Hide auth overlay, show app */
function enterApp() {
  const overlay = document.getElementById('auth-overlay');
  overlay.classList.remove('active');
  overlay.classList.add('hidden');
  document.getElementById('app').classList.remove('hidden');

  const name = currentUser.user_metadata?.full_name || currentUser.email.split('@')[0];
  document.getElementById('user-name-display').textContent = name;
  document.getElementById('user-avatar').textContent = name[0].toUpperCase();

  setGreeting(name);
  setHeaderDate();
  loadDemoData();
  renderDashboard();
  renderSchedule();
}

// ============================================================
//  NAVIGATION
// ============================================================
function showPage(pageId) {
  document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.nav-link').forEach(l => l.classList.remove('active'));
  document.getElementById(`page-${pageId}`).classList.add('active');
  document.querySelector(`[onclick="showPage('${pageId}')"]`).classList.add('active');

  if (pageId === 'progress') renderProgress();
}

// ============================================================
//  MODALS
// ============================================================
function openModal(id)  { document.getElementById(id).classList.add('open');    }
function closeModal(id) { document.getElementById(id).classList.remove('open'); }

// ============================================================
//  DASHBOARD
// ============================================================
function setGreeting(name) {
  const h = new Date().getHours();
  const part = h < 12 ? 'morning' : h < 17 ? 'afternoon' : 'evening';
  document.getElementById('greeting-text').textContent = `Good ${part}, ${name}!`;
}

function setHeaderDate() {
  const d = new Date();
  document.getElementById('header-date').innerHTML =
    `${d.toLocaleDateString('en-US', { weekday:'long', month:'long', day:'numeric' })}<br>${d.getFullYear()}`;
}

function renderDashboard() {
  const today = workouts.filter(w => isSameDay(new Date(w.date), new Date()));
  const week  = workouts.filter(w => isThisWeek(new Date(w.date)));

  const totalCal = today.reduce((s, w) => s + (w.calories || 0), 0);
  const totalMin = today.reduce((s, w) => s + (w.duration || 0), 0);

  document.getElementById('stat-calories').textContent = totalCal;
  document.getElementById('stat-steps').textContent    = (totalCal * 20).toLocaleString();
  document.getElementById('stat-duration').textContent = `${totalMin} min`;
  document.getElementById('stat-workouts').textContent = week.length;

  // Goal progress bars
  const steps   = totalCal * 20;
  const stepPct = Math.min(100, Math.round(steps / 10000 * 100));
  const calPct  = Math.min(100, Math.round(totalCal / 500 * 100));
  document.getElementById('pb-steps').style.width    = stepPct + '%';
  document.getElementById('pb-cal').style.width      = calPct + '%';
  document.getElementById('goal-steps-val').textContent = `${steps.toLocaleString()} / 10,000`;
  document.getElementById('goal-cal-val').textContent   = `${totalCal} / 500`;

  // Body metrics
  if (metrics.weight) {
    document.getElementById('metric-weight').textContent = metrics.weight;
    const bmi = metrics.height ? (metrics.weight / Math.pow(metrics.height / 100, 2)).toFixed(1) : '--';
    document.getElementById('metric-bmi').textContent    = bmi;
  }
  if (metrics.fat)    document.getElementById('metric-fat').textContent    = metrics.fat + '%';
  if (metrics.muscle) document.getElementById('metric-muscle').textContent = metrics.muscle + ' kg';

  renderWeeklyChart();
  renderRecentWorkouts();
}

function renderWeeklyChart() {
  const container = document.getElementById('weekly-chart');
  container.innerHTML = '';
  const days = ['Mon','Tue','Wed','Thu','Fri','Sat','Sun'];
  const today = new Date();
  const dayOfWeek = today.getDay(); // 0=Sun
  const monday = new Date(today);
  monday.setDate(today.getDate() - ((dayOfWeek + 6) % 7));

  const maxCal = 600;
  days.forEach((_, i) => {
    const d = new Date(monday);
    d.setDate(monday.getDate() + i);
    const dayCal = workouts
      .filter(w => isSameDay(new Date(w.date), d))
      .reduce((s, w) => s + (w.calories || 0), 0);
    const pct = Math.min(100, Math.round(dayCal / maxCal * 100));
    const bar = document.createElement('div');
    bar.className = 'bar';
    bar.style.cssText = `height:${pct}%; background:var(--accent-orange); opacity:${isSameDay(d, new Date()) ? 1 : 0.5};`;
    bar.title = `${days[i]}: ${dayCal} cal`;
    container.appendChild(bar);
  });
}

function renderRecentWorkouts() {
  const el = document.getElementById('recent-workouts-list');
  const recent = [...workouts].sort((a,b) => new Date(b.date) - new Date(a.date)).slice(0, 4);
  if (!recent.length) { el.innerHTML = '<p class="empty-state">No workouts yet. Start training!</p>'; return; }
  el.innerHTML = recent.map(w => `
    <div class="workout-item">
      <div class="wi-icon">${typeIcon(w.type)}</div>
      <div class="wi-info"><span class="wi-name">${w.name}</span><small>${w.duration} min · ${w.calories} cal</small></div>
      <span class="wi-badge">${w.intensity}</span>
    </div>`).join('');
}

// ============================================================
//  WORKOUTS
// ============================================================
function logWorkout() {
  const name      = document.getElementById('w-name').value.trim();
  const type      = document.getElementById('w-type').value;
  const duration  = parseInt(document.getElementById('w-duration').value) || 0;
  const calories  = parseInt(document.getElementById('w-calories').value) || 0;
  const intensity = document.getElementById('w-intensity').value;
  const notes     = document.getElementById('w-notes').value.trim();

  if (!name) { showToast('Please enter a workout name.', true); return; }

  const entry = { id: Date.now(), name, type, duration, calories, intensity, notes, date: new Date().toISOString() };
  workouts.push(entry);

  // --- Send to C++ backend ---
  apiFetch('/api/workouts', 'POST', entry);

  closeModal('workout-modal');
  clearForm(['w-name','w-duration','w-calories','w-notes']);
  renderWorkouts();
  renderDashboard();
  showToast('Workout logged! 💪');
}

function renderWorkouts() {
  const grid = document.getElementById('workouts-grid');
  if (!grid) return;
  const filter = document.querySelector('#page-workouts .filter-btn.active')?.dataset.filter || 'all';
  const filtered = filter === 'all' ? workouts : workouts.filter(w => w.type === filter);

  if (!filtered.length) {
    grid.innerHTML = `<div class="empty-card"><span>🏋️</span><p>No workouts yet.<br/>Log your first session!</p><button class="btn-primary" onclick="openModal('workout-modal')">Log Workout</button></div>`;
    return;
  }
  grid.innerHTML = filtered.map(w => `
    <div class="workout-card">
      <div class="wc-top">
        <div class="wc-icon">${typeIcon(w.type)}</div>
        <span class="wc-type">${w.type}</span>
        <button class="btn-text" style="margin-left:auto" onclick="deleteWorkout(${w.id})">✕</button>
      </div>
      <h4 class="wc-name">${w.name}</h4>
      <div class="wc-stats">
        <span>⏱ ${w.duration} min</span>
        <span>🔥 ${w.calories} cal</span>
        <span class="badge-intensity ${w.intensity}">${w.intensity}</span>
      </div>
      ${w.notes ? `<p class="wc-notes">${w.notes}</p>` : ''}
      <small style="color:var(--text-muted)">${formatDate(w.date)}</small>
    </div>`).join('');
}

function deleteWorkout(id) {
  workouts = workouts.filter(w => w.id !== id);
  apiFetch(`/api/workouts/${id}`, 'DELETE');
  renderWorkouts();
  renderDashboard();
  showToast('Workout deleted.');
}

// ============================================================
//  NUTRITION
// ============================================================
function logMeal() {
  const name  = document.getElementById('n-name').value.trim();
  const type  = document.getElementById('n-type').value;
  const cal   = parseInt(document.getElementById('n-cal').value) || 0;
  const prot  = parseInt(document.getElementById('n-prot').value) || 0;
  const carb  = parseInt(document.getElementById('n-carb').value) || 0;
  const fat   = parseInt(document.getElementById('n-fat-in').value) || 0;

  if (!name) { showToast('Please enter a meal name.', true); return; }

  const entry = { id: Date.now(), name, type, calories: cal, protein: prot, carbs: carb, fat, date: new Date().toISOString() };
  meals.push(entry);
  apiFetch('/api/meals', 'POST', entry);

  closeModal('nutrition-modal');
  clearForm(['n-name','n-cal','n-prot','n-carb','n-fat-in']);
  renderNutrition();
  showToast('Meal logged! 🥗');
}

function renderNutrition() {
  const list = document.getElementById('meal-list');
  if (!list) return;
  const todayMeals = meals.filter(m => isSameDay(new Date(m.date), new Date()));
  const totCal  = todayMeals.reduce((s,m) => s + m.calories, 0);
  const totProt = todayMeals.reduce((s,m) => s + m.protein, 0);
  const totCarb = todayMeals.reduce((s,m) => s + m.carbs, 0);
  const totFat  = todayMeals.reduce((s,m) => s + m.fat, 0);

  const macro = document.getElementById('macro-summary');
  if (macro) macro.innerHTML = `
    <div class="macro-item"><span class="macro-val">${totCal}</span><span class="macro-label">Calories</span></div>
    <div class="macro-item"><span class="macro-val">${totProt}g</span><span class="macro-label">Protein</span></div>
    <div class="macro-item"><span class="macro-val">${totCarb}g</span><span class="macro-label">Carbs</span></div>
    <div class="macro-item"><span class="macro-val">${totFat}g</span><span class="macro-label">Fat</span></div>`;

  if (!todayMeals.length) {
    list.innerHTML = '<p class="empty-state">No meals logged today.</p>'; return;
  }
  list.innerHTML = todayMeals.map(m => `
    <div class="meal-item">
      <div class="mi-top">
        <span class="mi-name">${m.name}</span>
        <span class="mi-type">${m.type}</span>
        <button class="btn-text" onclick="deleteMeal(${m.id})">✕</button>
      </div>
      <small>${m.calories} kcal · P:${m.protein}g C:${m.carbs}g F:${m.fat}g</small>
    </div>`).join('');
}

function deleteMeal(id) {
  meals = meals.filter(m => m.id !== id);
  apiFetch(`/api/meals/${id}`, 'DELETE');
  renderNutrition();
  showToast('Meal removed.');
}

// ============================================================
//  GOALS
// ============================================================
function addGoal() {
  const title   = document.getElementById('g-title').value.trim();
  const cat     = document.getElementById('g-cat').value;
  const date    = document.getElementById('g-date').value;
  const current = parseFloat(document.getElementById('g-current').value) || 0;
  const target  = parseFloat(document.getElementById('g-target').value) || 100;
  const unit    = document.getElementById('g-unit').value.trim();

  if (!title) { showToast('Please enter a goal title.', true); return; }

  const entry = { id: Date.now(), title, category: cat, targetDate: date, current, target, unit };
  goals.push(entry);
  apiFetch('/api/goals', 'POST', entry);

  closeModal('goal-modal');
  clearForm(['g-title','g-date','g-current','g-target','g-unit']);
  renderGoals();
  showToast('Goal set! 🎯');
}

function renderGoals() {
  const grid = document.getElementById('goals-grid');
  if (!grid) return;
  if (!goals.length) {
    grid.innerHTML = `<div class="empty-card"><span>🎯</span><p>No goals set yet.<br/>Define what you're working towards!</p><button class="btn-primary" onclick="openModal('goal-modal')">Set a Goal</button></div>`;
    return;
  }
  grid.innerHTML = goals.map(g => {
    const pct = Math.min(100, Math.round(g.current / g.target * 100));
    return `
    <div class="goal-card">
      <div class="goal-card-top">
        <span class="goal-card-title">${g.title}</span>
        <span class="goal-card-cat">${g.category}</span>
      </div>
      <div class="goal-card-progress">
        <div class="goal-pct">${pct}%</div>
        <div class="progress-bar" style="margin-top:6px"><div class="progress-fill green" style="width:${pct}%"></div></div>
      </div>
      <div class="goal-vals">${g.current} / ${g.target} ${g.unit}</div>
      ${g.targetDate ? `<div class="goal-date">Target: ${formatDate(g.targetDate)}</div>` : ''}
      <button class="btn-text" onclick="deleteGoal(${g.id})" style="margin-top:0.5rem">Remove</button>
    </div>`;
  }).join('');
}

function deleteGoal(id) {
  goals = goals.filter(g => g.id !== id);
  apiFetch(`/api/goals/${id}`, 'DELETE');
  renderGoals();
}

// ============================================================
//  METRICS
// ============================================================
function saveMetrics() {
  metrics.weight = parseFloat(document.getElementById('m-weight').value) || null;
  metrics.height = parseFloat(document.getElementById('m-height').value) || null;
  metrics.fat    = parseFloat(document.getElementById('m-fat').value) || null;
  metrics.muscle = parseFloat(document.getElementById('m-muscle').value) || null;

  apiFetch('/api/metrics', 'POST', metrics);
  closeModal('metrics-modal');
  renderDashboard();
  showToast('Metrics updated! 📊');
}

// ============================================================
//  SCHEDULE
// ============================================================
function addSchedule() {
  const name = document.getElementById('s-name').value.trim();
  const day  = document.getElementById('s-day').value;
  const time = document.getElementById('s-time').value;
  const dur  = parseInt(document.getElementById('s-dur').value) || 60;

  if (!name) { showToast('Please enter a session name.', true); return; }

  const entry = { id: Date.now(), name, day, time, duration: dur };
  schedules.push(entry);
  apiFetch('/api/schedules', 'POST', entry);

  closeModal('schedule-modal');
  clearForm(['s-name','s-time','s-dur']);
  renderSchedule();
  showToast('Session scheduled! 📅');
}

function renderSchedule() {
  const grid = document.getElementById('schedule-grid');
  if (!grid) return;
  const days = ['Monday','Tuesday','Wednesday','Thursday','Friday','Saturday','Sunday'];
  grid.innerHTML = days.map(day => {
    const sessions = schedules.filter(s => s.day === day);
    return `
    <div class="schedule-day ${sessions.length ? 'has-session' : ''}">
      <div class="schedule-day-label">${day.slice(0,3).toUpperCase()}</div>
      ${sessions.length ? sessions.map(s => `
        <div class="schedule-session">
          <div class="schedule-session-name">${s.name}</div>
          <div class="schedule-session-time">${s.time || '--:--'} · ${s.duration}min</div>
          <button class="btn-text" onclick="deleteSchedule(${s.id})" style="font-size:0.7rem">Remove</button>
        </div>`).join('') : '<p style="color:var(--text-muted);font-size:0.8rem">Rest day</p>'}
    </div>`;
  }).join('');
}

function deleteSchedule(id) {
  schedules = schedules.filter(s => s.id !== id);
  apiFetch(`/api/schedules/${id}`, 'DELETE');
  renderSchedule();
}

// ============================================================
//  PROGRESS PAGE
// ============================================================
function renderProgress() {
  const heatmap = document.getElementById('workout-heatmap');
  if (heatmap) {
    heatmap.innerHTML = '';
    for (let i = 0; i < 28; i++) {
      const d = new Date(); d.setDate(d.getDate() - 27 + i);
      const count = workouts.filter(w => isSameDay(new Date(w.date), d)).length;
      const cell = document.createElement('div');
      cell.className = `heat-cell${count >= 3 ? ' lvl3' : count === 2 ? ' lvl2' : count === 1 ? ' lvl1' : ''}`;
      cell.title = `${formatDate(d.toISOString())}: ${count} workout(s)`;
      heatmap.appendChild(cell);
    }
  }
}

// ============================================================
//  FILTER BUTTONS (Workouts)
// ============================================================
document.addEventListener('DOMContentLoaded', () => {
  document.querySelectorAll('.filter-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      btn.closest('.filter-bar').querySelectorAll('.filter-btn').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      renderWorkouts();
    });
  });

  // Re-render pages when tabs become active
  document.querySelectorAll('.nav-link').forEach(link => {
    link.addEventListener('click', () => {
      const page = link.getAttribute('onclick').match(/'(\w+)'/)[1];
      setTimeout(() => {
        if (page === 'workouts')   renderWorkouts();
        if (page === 'nutrition')  renderNutrition();
        if (page === 'goals')      renderGoals();
        if (page === 'progress')   renderProgress();
        if (page === 'schedule')   renderSchedule();
      }, 50);
    });
  });

  checkSession();
});

// ============================================================
//  SESSION RESTORE (on page reload)
// ============================================================
async function checkSession() {
  const { data: { session } } = await sb.auth.getSession();
  if (session) {
    currentUser = session.user;
    loadDemoData();
    enterApp();
  }
}

// ============================================================
//  DEMO DATA (shown until real backend data arrives)
// ============================================================
function loadDemoData() {
  const now = new Date().toISOString();
  const yesterday = new Date(); yesterday.setDate(yesterday.getDate() - 1);

  workouts = [
    { id: 1, name: 'Morning Run', type: 'cardio',    duration: 30, calories: 320, intensity: 'medium', notes: 'Felt great!', date: now },
    { id: 2, name: 'Bench Press', type: 'strength',  duration: 45, calories: 280, intensity: 'high',   notes: 'New PR!',    date: yesterday.toISOString() },
  ];
  meals = [
    { id: 1, name: 'Oats & Banana', type: 'breakfast', calories: 380, protein: 12, carbs: 65, fat: 7,  date: now },
    { id: 2, name: 'Chicken & Rice', type: 'lunch',    calories: 520, protein: 42, carbs: 50, fat: 10, date: now },
  ];
  goals = [
    { id: 1, title: 'Run 5km under 25 min', category: 'endurance', targetDate: '2025-12-31', current: 28, target: 25, unit: 'min' },
  ];
  metrics = { weight: 75, height: 178, fat: 16, muscle: 58 };
}

// ============================================================
//  C++ BACKEND API HELPER
//  When your C++ server is running, this sends requests to it.
//  If no backend, it silently fails (data stays in-memory).
// ============================================================
const API_BASE = 'http://localhost:8080'; // Change for production

async function apiFetch(path, method = 'GET', body = null) {
  try {
    const opts = {
      method,
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Bearer ${(await sb.auth.getSession()).data.session?.access_token || ''}`
      }
    };
    if (body) opts.body = JSON.stringify(body);
    const res = await fetch(API_BASE + path, opts);
    if (!res.ok) console.warn(`API ${method} ${path} → ${res.status}`);
    return await res.json().catch(() => null);
  } catch (e) {
    // Backend not running; data stays in memory
    console.info('Backend offline – using local state.');
    return null;
  }
}

// ============================================================
//  UTILITIES
// ============================================================
function isSameDay(a, b) {
  return a.getFullYear() === b.getFullYear() &&
         a.getMonth()    === b.getMonth()    &&
         a.getDate()     === b.getDate();
}

function isThisWeek(date) {
  const now     = new Date();
  const monday  = new Date(now);
  monday.setDate(now.getDate() - ((now.getDay() + 6) % 7));
  monday.setHours(0,0,0,0);
  return date >= monday;
}

function formatDate(iso) {
  return new Date(iso).toLocaleDateString('en-US', { month:'short', day:'numeric', year:'numeric' });
}

function typeIcon(type) {
  return { strength:'💪', cardio:'🏃', flexibility:'🧘', sports:'⚽' }[type] || '🏋️';
}

function clearForm(ids) {
  ids.forEach(id => {
    const el = document.getElementById(id);
    if (el) el.value = '';
  });
}

function showToast(msg, isError = false) {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.className = 'toast show' + (isError ? ' error' : '');
  setTimeout(() => t.classList.remove('show'), 3000);
}
