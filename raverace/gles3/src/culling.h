static unsigned p3_cull(const M3Quad *r)
{
    double low[4][3], high[4][3], zoom = ldexp((double)r->meta[24], -r->meta[25]);
    for (int v = 0; v < 4; v++)
        for (int c = 0; c < 3; c++) {
            int64_t dot = 0;
            double absolute = 0;
            for (int k = 0; k < 3; k++) {
                int64_t term = (int64_t)r->local[v * 6 + k] * r->meta[12 + k * 3 + c];
                dot += term;
                absolute += fabs((double)term);
            }
            double tr = r->meta[21 + c], error = absolute / (32768. * 1048576.) + fabs(tr) / 1048576. + 2;
            double xyz = (double)(dot >> 15) + tr;
            low[v][c] = xyz - error;
            high[v][c] = xyz + error;
        }
    for (int v = 0; v < 4; v++)
        if (low[v][2] <= 1)
            return 0;
    bool outside[4] = {true, true, true, true};
    for (int v = 0; v < 4; v++) {
        double xl = 1e100, xh = -1e100, yl = 1e100, yh = -1e100;
        if (isfinite(zoom) && zoom >= 0) {
            double zl = low[v][2], zh = high[v][2];
            xl = fmin(xl, low[v][0] / (low[v][0] < 0 ? zl : zh) * zoom);
            xh = fmax(xh, high[v][0] / (high[v][0] < 0 ? zh : zl) * zoom);
            yl = fmin(yl, low[v][1] / (low[v][1] < 0 ? zl : zh) * zoom);
            yh = fmax(yh, high[v][1] / (high[v][1] < 0 ? zh : zl) * zoom);
        } else {
            for (int a = 0; a < 2; a++)
                for (int b = 0; b < 2; b++) {
                    double z = b ? high[v][2] : low[v][2], x = (a ? high[v][0] : low[v][0]) / z * zoom,
                           y = (a ? high[v][1] : low[v][1]) / z * zoom;
                    xl = fmin(xl, x);
                    xh = fmax(xh, x);
                    yl = fmin(yl, y);
                    yh = fmax(yh, y);
                }
        }
        outside[0] &= r->meta[26] + xh < r->meta[28] - 4;
        outside[1] &= r->meta[26] + xl > r->meta[29] + 4;
        outside[2] &= r->meta[27] - yl < r->meta[30] - 4;
        outside[3] &= r->meta[27] - yh > r->meta[31] + 4;
    }
    for (unsigned i = 0; i < 4; i++)
        if (outside[i])
            return i + 1;
    return 0;
}
