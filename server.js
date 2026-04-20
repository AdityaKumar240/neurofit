/*
 * NeuroFit AI — Node.js Proxy Server
 * Routes AI requests securely to Groq API
 * Deploy on Railway (free tier)
 */

const express  = require('express');
const cors     = require('cors');
const fetch    = require('node-fetch');

const app = express();
app.use(cors());
app.use(express.json({ limit: '10mb' }));

// Health check
app.get('/', (req, res) => {
  res.json({
    status:   'NeuroFit AI Proxy — Online ✅',
    provider: 'Groq (Llama-3.3-70B)',
    version:  '2.0.0'
  });
});

// Main AI proxy endpoint
app.post('/api/claude', async (req, res) => {
  try {
    const { messages, system, model } = req.body;

    if (!messages || !Array.isArray(messages)) {
      return res.status(400).json({ error: { message: 'messages array is required' } });
    }

    // Build Groq message array
    const groqMessages = [];
    if (system) groqMessages.push({ role: 'system', content: system });
    groqMessages.push(...messages);

    // Call Groq API
    const response = await fetch('https://api.groq.com/openai/v1/chat/completions', {
      method: 'POST',
      headers: {
        'Content-Type':  'application/json',
        'Authorization': `Bearer ${process.env.GROQ_API_KEY}`
      },
      body: JSON.stringify({
        model:       'llama-3.3-70b-versatile',
        messages:    groqMessages,
        max_tokens:  1200,
        temperature: 0.7
      })
    });

    const data = await response.json();

    // Log for debugging
    if (process.env.DEBUG === 'true') {
      console.log('[GROQ]', JSON.stringify(data).slice(0, 300));
    }

    // Handle Groq errors
    if (data.error) {
      console.error('[GROQ ERROR]', data.error);
      return res.status(400).json({ error: { message: data.error.message } });
    }

    if (!data.choices?.[0]?.message?.content) {
      console.error('[GROQ] Unexpected response:', data);
      return res.status(500).json({ error: { message: 'Unexpected response from AI provider' } });
    }

    // Return in Anthropic-compatible format
    // (frontend expects: { content: [{ type: 'text', text: '...' }] })
    res.json({
      content: [{
        type: 'text',
        text: data.choices[0].message.content
      }],
      model:  'llama-3.3-70b-versatile',
      usage:  data.usage || {}
    });

  } catch (err) {
    console.error('[SERVER ERROR]', err.message);
    res.status(500).json({ error: { message: 'Internal proxy error: ' + err.message } });
  }
});

// Start server
const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`\n  NeuroFit AI Proxy running on port ${PORT}`);
  console.log(`  Provider: Groq (Llama-3.3-70B)`);
  console.log(`  Health: http://localhost:${PORT}/\n`);
});
