# Model report

Horizon H = 15 s, window = 10 s, features = temp_rise, temp_slope10, temp_slope3, power_now, power_avg10, power_slope10, workload.
Thresholds: caution +5.5 C / critical +8.9 C above idle baseline;
power caution 160.1 mW / critical 210.3 mW.
Train rows 8082, test rows 2694 (last 25 % of every recording segment).
Model: 502 parameters (~2.0 KB of float weights); seed 44 kept out of 5 by lowest validation loss.

## Temperature risk (test set)

| Predictor | Accuracy | Macro F1 | False alarm rate | Critical recall | Critical missed as Normal |
|---|---|---|---|---|---|
| ML model | 0.955 | 0.939 | 0.016 | 0.984 | 0.001 |
| Slope rule | 0.924 | 0.895 | 0.006 | 0.975 | 0.002 |

CRITICAL warning lead time before the limit was actually crossed (ideal is about H):
- ML model: mean 14.5 s, median 15.5 s over 8 crossings
- Slope rule: mean 20.0 s, median 19.5 s over 8 crossings

## Power risk (test set)

| Predictor | Accuracy | Macro F1 | False alarm rate | Critical recall | Critical missed as Normal |
|---|---|---|---|---|---|
| ML model | 0.934 | 0.895 | 0.059 | 0.984 | 0.007 |
| Slope rule | 0.877 | 0.759 | 0.071 | 0.978 | 0.017 |

Confusion matrices (rows = true NORMAL/CAUTION/CRITICAL, columns = predicted):
```
temp  ml  : [[819, 13, 0], [65, 435, 21], [2, 19, 1320]]
temp  rule: [[827, 5, 0], [75, 355, 91], [3, 30, 1308]]
power ml  : [[963, 26, 34], [45, 240, 52], [10, 11, 1313]]
power rule: [[950, 34, 39], [166, 108, 63], [23, 6, 1305]]
```
