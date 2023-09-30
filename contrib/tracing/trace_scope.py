from __future__ import print_function
from bcc import BPF, USDT
import sys

if len(sys.argv) < 2:
    print("USAGE: trace_scope.py PID")
    exit()
pid = sys.argv[1]
debug = 1

# load BPF program
bpf_text = """
#include <uapi/linux/ptrace.h>

BPF_HASH(data);

int scope_enter(struct pt_regs* ctx) {
    u32 key_start = 0;
    u32 key_duration = 1;
    
    // attempt to read stored timestamp
    u64* start_time_ns = data.lookup(&key_start);
    if (start_time_ns) {
        u64 zero = 0;
        u64* duration = data.lookup_or_try_init(&key_duration, &zero);
        if (duration) {
            data.update(&key_duration, *duration + (*start_time_ns - bpf_ktime_get_ns()));
        }
        data.delete(&key_start);
    }
    return 0;
};

int scope_end(struct pt_regs* ctx) {
    uint32_t key_start = 0;
    data.update(&key_start, bpf_ktime_get_ns());
}
"""

# enable USDT probe from given PID
u = USDT(pid=int(pid))
u.enable_probe(
    probe="block_manager:read_block_from_disk_unserialize_enter",
    fn_name="scope_enter")
u.enable_probe(
    probe="block_manager:read_block_from_disk_unserialize_exit",
    fn_name="scope_exit")
if debug:
    print(u.get_text())
    print(bpf_text)

# initialize BPF
b = BPF(text=bpf_text, usdt_contexts=[u])
