seq - sequential variant
parallel - separate buffers, not copied to back/front, if-ing in Workspace::get
parallel_lb - no overlapping + separate comm buffers, but contents copied to large buffer
parallel_async - overlapped computations, but separate buffers (copied to front/back)
parallel_gap - overlapping, all transfers to directly to front/back buffer
parallel_ts - gaped transfers directly to front/back buffer, with time intervals (fetch additional data to avoid communiation)

modification summary:
1. separate buffers, iffing [parallel] ->
    separate buffers, copying [_lb, _async]] ->
    integrated buffers, gapped transfers [_gap, _ts]

2. non-overlapped transfers -> [parallel, _lb] -> overlapped [_async, _gap, _ts]

3. no time intervals [everyhing but _ts] -> time intervals [_ts]