#!/bin/bash

echo "Starting Chrono Rift..."

./arbiter_out &
sleep 2

./asp_out &
sleep 1

./render_out &
sleep 1

./hip2_out &
sleep 1

# Keep the interactive HIP in foreground
./hip_out

wait