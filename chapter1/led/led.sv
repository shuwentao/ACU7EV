`timescale 1ns / 1ps
////////////////////////////////////////////////////////////////////////////////
// Company:
// Engineer:
//
// Create Date: 09/15/2026 03:51:52 PM
// Design Name:
// Module Name: led
// Project Name:
// Target Devices:
// Tool Versions:
// Description: Blink an LED at a fixed interval, driven by a differential
//              system clock buffered through IBUFDS.
//
// Dependencies: None
//
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
////////////////////////////////////////////////////////////////////////////////


module led (
    // Differential system clock
    input  logic sys_clk_p,
    input  logic sys_clk_n,
    input  logic rst_n,
    // LED output, marked for debug so that an ILA can capture it
    output logic led
);

    // 200 MHz input clock: toggle the LED every 1 s, i.e. a 2 s blink period.
    localparam int unsigned CLK_FREQ_HZ   = 200_000_000;
    localparam int unsigned TOGGLE_CYCLES = CLK_FREQ_HZ;  // 1 s worth of cycles
    localparam int unsigned CNT_MAX       = TOGGLE_CYCLES - 1;

    // Marked for debug: this net survives synthesis and can be probed by an ILA,
    // otherwise the tool is free to optimise it away or rename it.
    logic [31:0] timer_cnt;

    // Buffered system clock, driven by the IBUFDS instance below
    logic sys_clk;

    // Differential clock input buffer
    IBUFDS u_ibufds (
        .O (sys_clk),    // 1-bit output: Buffer output
        .I (sys_clk_p),  // 1-bit input : Diff_p buffer input (connect directly to top-level port)
        .IB(sys_clk_n)   // 1-bit input : Diff_n buffer input (connect directly to top-level port)
    );

    // Divider: free-running counter, rolls over every 1 s
    always_ff @(posedge sys_clk) begin
        if (!rst_n)
            timer_cnt <= 32'd0;
        else if (timer_cnt >= CNT_MAX)
            timer_cnt <= 32'd0;
        else
            timer_cnt <= timer_cnt + 1'b1;
    end

    // LED: toggle every time the divider wraps
    always_ff @(posedge sys_clk) begin
        if (!rst_n)
            led <= 1'b0;
        else if (timer_cnt >= CNT_MAX)
            led <= ~led;
    end

endmodule


