args <- commandArgs(trailingOnly = TRUE)
if (length(args) != 2) {
  stop("Usage: Rscript plot_drive_cycle.R POWER_CSV OUTPUT_PNG")
}

trace <- read.csv(args[1])
png(args[2], width = 1440, height = 790, res = 150, type = "cairo", bg = "#F9F9F6")
par(mar = c(4.5, 5, 4.8, 2), las = 1, family = "sans",
    col.axis = "#485357", col.lab = "#1D2B2F", xaxs = "i", yaxs = "i")

plot(trace$time_s, trace$wheel_w, type = "n", xlim = c(0, 8),
     ylim = c(-165, 145), xlab = "Time (s)", ylab = "Power (W)", axes = FALSE)
rect(2, -165, 6, 145, col = "#E4F0EA", border = NA)
abline(h = seq(-150, 100, 50), col = "#D7DEDA", lwd = 0.7)
abline(h = 0, col = "#829397")
abline(v = sqrt(52), col = "#839395", lty = 3, lwd = 1.2)
axis(1, at = 0:8, col = "#859497")
axis(2, at = seq(-150, 100, 50), col = "#859497")

lines(trace$time_s, trace$traction_w, col = "#006855", lwd = 3)
lines(trace$time_s, trace$wheel_w, col = "#425A7E", lwd = 1.65, lty = 2)
text(4, 55, "Recovery limited to 96 W", col = "#006855", cex = 0.9)
text(6.95, 93, "Propulsion starts", adj = 1, col = "#485357", cex = 0.84)
legend("topleft", c("DC traction", "Wheel"), inset = c(0.018, 0.035),
       col = c("#006855", "#425A7E"), lwd = c(3, 1.65), lty = c(1, 2),
       bty = "n", text.col = "#1D2B2F")

mtext("Power during the synthetic braking interval", side = 3, line = 2.75,
      adj = 0, font = 2, cex = 1.5, col = "#1D2B2F")
mtext("Speed ramps from 0 to 8 m/s on a fixed grade. Negative power indicates braking.",
      side = 3, line = 1.3, adj = 0, cex = 0.9, col = "#485357")
invisible(dev.off())
