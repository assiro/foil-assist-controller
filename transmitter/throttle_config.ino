// Throttle curve functions
// 0 <= throttle <= 100

// extreme throttle curve
int extreme_throttle_curve(float throttle) {
  return 9*throttle/(0.065*throttle + 2.5);
}

// aggressive throttle curve
int strong_throttle_curve(float throttle) {
  return 9*throttle/(0.058*throttle + 3.2);
}

// mid throttle curve regulation
int mid_throttle_curve(float throttle) {
  return 9*throttle/(0.05*throttle + 4);
}

// soft throttle curve regulation
int soft_throttle_curve(float throttle) {
  return 9*throttle/(0.03*throttle + 6);
}

int angular_throttle_curve(float throttle) {
  return 9*throttle/(0.03*throttle + 6);
}

int eco_throttle_curve(float throttle) {
  return 3.2*throttle/(9 - 0.058*throttle);
}