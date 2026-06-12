// Frij AI — the assistant's brain, as a Supabase Edge Function.
//
// The device (or emulator) POSTs a question; this function calls Gemini
// (free tier) with tool declarations that read/write the Frij store table,
// runs the tool loop, and returns {"q": ..., "a": ...} for the watch to show.
// The GEMINI_API_KEY never leaves this function.
//
//   POST /functions/v1/ask          (Authorization: Bearer <anon key>)
//     {"q": "what's on this week?"}                    — text ask
//     {"audio": "<base64>", "mime": "audio/wav"}       — voice ask (device)
//
// Secrets:  supabase secrets set GEMINI_API_KEY=...   (aistudio.google.com)
// Optional: GEMINI_MODEL (default gemini-2.5-flash)

import { createClient } from "npm:@supabase/supabase-js@2";

const GEMINI_KEY = Deno.env.get("GEMINI_API_KEY") ?? "";
const MODEL = Deno.env.get("GEMINI_MODEL") ?? "gemini-2.5-flash";
const TABLE = Deno.env.get("FRIJ_TABLE") ?? "store";

// Service-role client: the function may write rows (scoreboard, todo inbox).
const db = createClient(
  Deno.env.get("SUPABASE_URL")!,
  Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!,
);

async function storeGet(key: string): Promise<unknown> {
  const { data } = await db.from(TABLE).select("value").eq("key", key).maybeSingle();
  return data?.value ?? null;
}

async function storeSet(key: string, value: unknown): Promise<void> {
  await db.from(TABLE).upsert(
    { key, value, updated_at: new Date().toISOString() },
    { onConflict: "key" },
  );
}

// ---- tools the model may call ----------------------------------------------

const TOOLS = [{
  functionDeclarations: [
    {
      name: "get_events",
      description:
        "Upcoming family calendar events and public holidays, soonest first. " +
        "Each has title t, date d (YYYY-MM-DD), optional start/end time tm/te, " +
        "optional location l, and h=true when it is a public holiday.",
    },
    {
      name: "get_todos",
      description:
        "The shared todo list. Each item has text t and done flag d.",
    },
    {
      name: "add_todo",
      description:
        "Add a new item to the shared todo list (it syncs to Google Keep on " +
        "the next bridge run). Use for 'add X to the list' requests.",
      parameters: {
        type: "object",
        properties: { text: { type: "string", description: "the item text" } },
        required: ["text"],
      },
    },
    {
      name: "add_point",
      description: "Add N points (default 1) to a scoreboard player.",
      parameters: {
        type: "object",
        properties: {
          player: { type: "string", enum: ["Farzin", "Farah"] },
          points: { type: "number" },
        },
        required: ["player"],
      },
    },
  ],
}];

async function runTool(name: string, args: Record<string, unknown>): Promise<unknown> {
  switch (name) {
    case "get_events": {
      const v = (await storeGet("events")) as { ev?: unknown } | unknown[] | null;
      return { events: Array.isArray(v) ? v : (v?.ev ?? []) };
    }
    case "get_todos":
      return { todos: (await storeGet("todo")) ?? [] };
    case "add_todo": {
      // The Keep bridge owns the list structure, so new items land in a
      // separate inbox row the bridge will push to Keep (then they appear in
      // store:todo like any other item).
      const inbox = ((await storeGet("todo_inbox")) as string[] | null) ?? [];
      inbox.push(String(args.text ?? "").slice(0, 63));
      await storeSet("todo_inbox", inbox);
      return { ok: true, queued: args.text };
    }
    case "add_point": {
      const key = args.player === "Farah" ? "sb_b" : "sb_a";
      const cur = parseInt(String((await storeGet(key)) ?? "0"), 10) || 0;
      const next = cur + (typeof args.points === "number" ? args.points : 1);
      await storeSet(key, String(next));
      return { ok: true, player: args.player, score: next };
    }
    default:
      return { error: `unknown tool ${name}` };
  }
}

// ---- Gemini ------------------------------------------------------------------

const SYSTEM = `You are Frij, a tiny voice assistant living on a round smart
display on a family's fridge in Malaysia. The family: Farzin and Farah.
Answer in 1-3 short sentences — the screen is small. Be warm but brief.
Answer ANY everyday question (cooking, conversions, time, general knowledge)
directly from your own knowledge. Use the tools only when the question is
about the family's calendar, todo list, or scoreboard. Never refuse a
reasonable kitchen or household question. Today is ${new Date().toDateString()}.
Your FINAL reply must be exactly this JSON (no markdown, no fences):
{"q": "<the user's question, transcribed if it came as audio>", "a": "<your answer>"}`;

type Part = Record<string, unknown>;

async function gemini(contents: unknown[]): Promise<Part[]> {
  // The free tier throws transient 503s under load — retry a couple of times.
  for (let attempt = 0; ; attempt++) {
    const r = await fetch(
      `https://generativelanguage.googleapis.com/v1beta/models/${MODEL}:generateContent?key=${GEMINI_KEY}`,
      {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          contents,
          tools: TOOLS,
          systemInstruction: { parts: [{ text: SYSTEM }] },
        }),
      },
    );
    if (r.ok) {
      const body = await r.json();
      return body.candidates?.[0]?.content?.parts ?? [];
    }
    if (attempt < 2 && (r.status === 503 || r.status === 429)) {
      await new Promise((res) => setTimeout(res, 1200 * (attempt + 1)));
      continue;
    }
    // friendly, device-sized text (never leak the raw provider JSON)
    throw new Error(
      r.status === 503 || r.status === 429
        ? "Frij AI is busy right now. Try again in a moment."
        : "Frij AI couldn't answer that. Please try again.",
    );
  }
}

// ---- handler ------------------------------------------------------------------

Deno.serve(async (req) => {
  if (req.method !== "POST") {
    return new Response("POST only", { status: 405 });
  }
  if (!GEMINI_KEY) {
    return Response.json(
      { error: "GEMINI_API_KEY is not set (supabase secrets set GEMINI_API_KEY=...)" },
      { status: 503 },
    );
  }

  let q = "";
  const userParts: Part[] = [];
  try {
    const body = await req.json();
    if (typeof body.q === "string" && body.q.trim()) {
      q = body.q.trim().slice(0, 300);
      userParts.push({ text: q });
    } else if (typeof body.audio === "string" && body.audio) {
      userParts.push({
        inlineData: { mimeType: body.mime ?? "audio/wav", data: body.audio },
      });
      userParts.push({ text: "(voice question — transcribe it into the q field)" });
    } else {
      return Response.json({ error: "send {q} or {audio}" }, { status: 400 });
    }
  } catch {
    return Response.json({ error: "bad json" }, { status: 400 });
  }

  try {
    const contents: unknown[] = [{ role: "user", parts: userParts }];
    let parts = await gemini(contents);

    // tool loop (capped — a fridge question shouldn't need more)
    for (let round = 0; round < 4; round++) {
      const calls = parts.filter((p) => p.functionCall) as
        { functionCall: { name: string; args?: Record<string, unknown> } }[];
      if (calls.length === 0) break;
      contents.push({ role: "model", parts });
      const responses = [];
      for (const c of calls) {
        responses.push({
          functionResponse: {
            name: c.functionCall.name,
            response: await runTool(c.functionCall.name, c.functionCall.args ?? {}),
          },
        });
      }
      contents.push({ role: "user", parts: responses });
      parts = await gemini(contents);
    }

    const text = parts.map((p) => (p.text as string) ?? "").join("").trim();
    // the system prompt demands JSON; tolerate fences or a bare-text slip
    let out = { q, a: text };
    const m = text.match(/\{[\s\S]*\}/);
    if (m) {
      try {
        const j = JSON.parse(m[0]);
        out = { q: j.q || q || "(voice)", a: String(j.a ?? text) };
      } catch { /* keep the bare-text fallback */ }
    }
    return Response.json(out);
  } catch (e) {
    return Response.json({ error: (e instanceof Error ? e.message : String(e)).slice(0, 160) }, { status: 502 });
  }
});
