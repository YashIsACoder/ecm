args <- commandArgs(trailingOnly = TRUE)
if (length(args) < 2 || length(args) > 3) {
  stop("Usage: Rscript analysis/plot.R trials.csv chart.png [route caption]", call. = FALSE)
}
caption <- if (length(args) == 3) args[3] else "synthetic route and vehicle"

trials <- read.csv(args[1])
policies <- c("steady", "push", "reserve-first")
completion <- tapply(trials$completed, trials$policy, mean)[policies]
below_reserve <- tapply(trials$reserve_breached, trials$policy, mean)[policies]
labels <- c("Steady", "Push", "Reserve-first")
colours <- c("#416779", "#B95538", "#258468")

png(args[2], width = 1600, height = 720, res = 160, type = "cairo", bg = "#F6F5F0")
par(family = "sans", mfrow = c(1, 2), oma = c(3.9, 0.4, 4.0, 0.4),
    mar = c(4.0, 1.0, 3.0, 1.9), col.axis = "#4A5A53", col.main = "#1B2A23",
    las = 1, mgp = c(1.7, 0.45, 0))

panel <- function(values, title, subtitle) {
  positions <- barplot(rev(values) * 100, horiz = TRUE, xlim = c(0, 110), ylim = c(0.35, 5.65),
                       axes = FALSE, axisnames = FALSE, space = 0.71, border = NA, col = rev(colours))
  axis(1, at = seq(0, 100, 25), labels = paste0(seq(0, 100, 25), "%"),
       lwd = 0, lwd.ticks = 1, col.ticks = "#ABB7AE", cex.axis = 0.85)
  text(rep(0, 3), positions + 0.76, rev(labels), pos = 4, offset = 0,
       cex = 0.87, col = "#22342B")
  text(rev(values) * 100, positions, sprintf("%.1f%%", rev(values) * 100), pos = 4,
       offset = 0.34, cex = 0.82, col = "#22342B", xpd = NA)
  mtext(title, side = 3, at = 0, adj = 0, line = 1.8, font = 2, cex = 1.24)
  mtext(subtitle, side = 3, at = 0, adj = 0, line = 0.55, cex = 0.76, col = "#4A5A53")
}

panel(completion, "Finished the route", "Reached the end without depleting the battery")
panel(below_reserve, "Went below reserve", "State of charge fell below 15% at least once")
mtext("Pacing the outback", outer = TRUE, side = 3, adj = 0.024, line = 1.7,
      font = 2, cex = 1.75, col = "#1B2A23")
mtext(paste0(format(length(unique(trials$trial)), big.mark = ","), " paired weather draws  |  ", caption),
      outer = TRUE, side = 3, adj = 0.024, line = 0.15, cex = 0.94, col = "#4A5A53")
mtext("Results apply to the synthetic route, vehicle and weather assumptions shown here.",
      outer = TRUE, side = 1, adj = 0.024, line = 0.6, cex = 0.82, col = "#4A5A53")
invisible(dev.off())
