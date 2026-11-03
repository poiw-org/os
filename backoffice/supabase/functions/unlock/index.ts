// Remote unlock for members. An admin talks to the door's realtime channel from the browser; a
// member must not, because that channel also carries key tokens and the write-card commands. So a
// member asks this function instead: it checks that the caller may unlock (an admin, or a user with
// an active key), sends `force_open` on the door's channel, waits for the door's confirmation and
// logs the open under the caller's name.
//
// POST {} -> { latency_ms, delay_ms, logged }   (delay_ms: the door unlocks this long after confirming)
import { createClient } from "npm:@supabase/supabase-js@2";
import {
  accessToken,
  callerId,
  env,
  HttpError,
  handlePost,
  serviceClient,
} from "../_shared/http.ts";

const DOOR_ID = Deno.env.get("DOOR_ID") ?? "front";
const ANSWER_TIMEOUT_MS = 6000;
// The door acts on each command id once, so resending until it answers is harmless and covers a
// door that is reconnecting when the first copy goes out.
const RESEND_MS = 700;

async function commandDoor(db: ReturnType<typeof serviceClient>, id: string) {
  const channel = db.channel(`door:${DOOR_ID}`, { config: { private: true } });
  let timer: number | undefined;
  let resend: number | undefined;
  const acked = new Promise<{ delay_ms?: number }>((resolve) =>
    channel.on("broadcast", { event: "force_open_ack" }, ({ payload }) => {
      if (payload?.id === id) resolve(payload);
    }),
  );
  const timedOut = new Promise<never>((_, reject) => {
    timer = setTimeout(
      () =>
        reject(
          new HttpError(
            504,
            "The door did not answer, so nothing was unlocked. It may be offline.",
          ),
        ),
      ANSWER_TIMEOUT_MS,
    );
  });

  try {
    await Promise.race([
      new Promise<void>((resolve, reject) =>
        channel.subscribe((status) => {
          if (status === "SUBSCRIBED") resolve();
          else if (
            status === "CHANNEL_ERROR" ||
            status === "TIMED_OUT" ||
            status === "CLOSED"
          )
            reject(new HttpError(502, "Could not reach the door."));
        }),
      ),
      timedOut,
    ]);

    const command = {
      type: "broadcast" as const,
      event: "force_open",
      payload: { id, ts: Date.now() },
    };
    const sentAt = Date.now();
    void channel.send(command);
    resend = setInterval(() => void channel.send(command), RESEND_MS);
    const ack = await Promise.race([acked, timedOut]);
    return {
      latencyMs: Date.now() - sentAt,
      delayMs: Number(ack.delay_ms) || 0,
    };
  } finally {
    clearTimeout(timer);
    clearInterval(resend);
    await db.removeChannel(channel);
  }
}

Deno.serve((req) =>
  handlePost(req, async () => {
    const token = accessToken(req);
    const db = serviceClient();
    await callerId(db, token);

    // The caller's own identity decides, through the same rule the database applies when logging.
    const asCaller = createClient(
      env("SUPABASE_URL"),
      env("SUPABASE_ANON_KEY"),
      {
        auth: { persistSession: false },
        global: { headers: { Authorization: `Bearer ${token}` } },
      },
    );
    const { data: allowed, error } = await asCaller.rpc("can_unlock");
    if (error) throw new HttpError(500, "Could not check permissions");
    if (!allowed)
      throw new HttpError(403, "You need an active key to unlock the door");

    const id = crypto.randomUUID();
    const { latencyMs, delayMs } = await commandDoor(db, id);

    // The door has confirmed; a failure to log must not read as a failure to unlock.
    const { error: logError } = await asCaller.rpc("log_web_open", {
      door: DOOR_ID,
      command_id: id,
      latency_ms: latencyMs,
    });
    if (logError) console.error(logError);
    return { latency_ms: latencyMs, delay_ms: delayMs, logged: !logError };
  }),
);
