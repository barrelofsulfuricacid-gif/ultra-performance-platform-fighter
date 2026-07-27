# M0 representation benchmark summary

Source: `performance/m0_representation/results.csv`

Cases: 23; samples: 345. Higher throughput is better. The relative column is the median paired ratio to the named family baseline with a deterministic 20,000-resample, two-sided 95% bootstrap confidence interval.

| Family | Candidate | Median throughput | MAD | Relative to baseline (95% CI) | State bytes | Unit |
|---|---|---:|---:|---:|---:|---|
| broadphase_dense | bitboard_256x256 | 21.088 M | 1.43% | 1.564x [1.547, 1.626] | 8192 | attacker_queries |
| broadphase_dense | grid_16x16 | 35.628 M | 1.68% | 2.678x [2.556, 2.764] | 2048 | attacker_queries |
| broadphase_dense | naive | 13.387 M | 1.47% | 1.000x [1.000, 1.000] | 0 | attacker_queries |
| broadphase_dense | sweep_rebuild | 10.416 M | 0.67% | 0.777x [0.762, 0.799] | 0 | attacker_queries |
| broadphase_sparse | bitboard_256x256 | 41.681 M | 0.86% | 0.813x [0.808, 0.823] | 8192 | attacker_queries |
| broadphase_sparse | grid_16x16 | 80.394 M | 2.41% | 1.579x [1.564, 1.603] | 2048 | attacker_queries |
| broadphase_sparse | naive | 50.937 M | 1.86% | 1.000x [1.000, 1.000] | 0 | attacker_queries |
| broadphase_sparse | sweep_rebuild | 83.472 M | 1.71% | 1.625x [1.591, 1.655] | 0 | attacker_queries |
| layout_update | aos_with_cold | 593.353 M | 1.68% | 1.000x [1.000, 1.000] | 2359296 | entity_updates |
| layout_update | hot_cold_split | 811.262 M | 2.08% | 1.377x [1.331, 1.391] | 2359296 | entity_updates |
| layout_update | soa | 4.190 G | 1.03% | 7.076x [6.966, 7.193] | 2359296 | entity_updates |
| numeric_motion | cell_256_int8 | 522.885 M | 1.93% | 0.636x [0.626, 0.640] | 16384 | fighter_ticks |
| numeric_motion | fixed_q16_16 | 1.170 G | 1.35% | 1.410x [1.389, 1.440] | 65536 | fighter_ticks |
| numeric_motion | float32 | 821.264 M | 0.77% | 1.000x [1.000, 1.000] | 65536 | fighter_ticks |
| numeric_motion | hybrid_int_position_float_velocity | 115.845 M | 1.75% | 0.141x [0.138, 0.147] | 98304 | fighter_ticks |
| snapshot_64k | full_copy_restore | 348.387 k | 1.23% | 1.000x [1.000, 1.000] | 65536 | snapshots |
| snapshot_64k | scan_delta_64byte_chunks | 644.458 k | 1.59% | 1.866x [1.827, 1.905] | 65536 | snapshots |
| snapshot_64k | tracked_dirty_8x64 | 29.181 M | 1.16% | 83.812x [83.178, 84.259] | 528 | snapshots |
| state_dispatch | data_table | 4.675 G | 1.26% | 2.893x [2.752, 2.937] | 294912 | entity_dispatches |
| state_dispatch | function_table | 123.890 M | 0.70% | 0.076x [0.074, 0.077] | 294912 | entity_dispatches |
| state_dispatch | switch | 1.635 G | 2.28% | 1.000x [1.000, 1.000] | 294912 | entity_dispatches |
| world_resolution | world_256_u8 | 924.438 M | 1.63% | 1.000x [1.000, 1.000] | 16384 | fighter_ticks |
| world_resolution | world_4096_u16 | 950.362 M | 1.62% | 1.033x [1.017, 1.055] | 32768 | fighter_ticks |
