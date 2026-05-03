`timescale 1ns/1ps
`define DIAG(a, b) (a+b)

module smoke_tb;
  localparam
    R          = 2,   // Rows of SA == rows of output matrix
    C          = 2,   // Cols of SA == cols of output matrix
    K          = 6,   // Cols of matrix_k and rows of matrix_x
    WX         = 8,   // word width of matrix_x
    WK         = 4,   // word width of matrix_k
    LM         = 1,   // latency of multiplier for smoke test
    LA         = 1,   // latency of accumulator for smoke test
    WM         = WX + WK,
    WY         = WM + $clog2(K),
    P_VALID    = 1,
    P_READY    = 100,
    CLK_PERIOD = 100;

  logic clk = 0;
  logic rstn = 0;

  initial forever #(CLK_PERIOD/2) clk = ~clk;

  // Systolic Array interface
  logic s_ready;
  logic s_valid = 0;
  logic s_last  = 0;

  logic m_ready = 0;
  logic m_valid;
  logic m_last;

  logic [R-1:0][WX-1:0] sx_data;
  logic [C-1:0][WK-1:0] sk_data;
  logic [R-1:0][WY-1:0] m_data;

  axis_sa #(
    .R(R),
    .C(C),
    .WX(WX),
    .WK(WK),
    .WY(WY),
    .LM(LM),
    .LA(LA)
  ) DUT (.*);

  logic signed [K-1:0][C-1:0][WK-1:0] mat_k;
  logic signed [K-1:0][R-1:0][WX-1:0] mat_x;
  logic signed [C-1:0][R-1:0][WY-1:0] mat_y_sim;
  logic signed [C-1:0][R-1:0][WY-1:0] mat_y_ref = '0;

  // y(C,R) = k(K,C).T @ x(K,R)
  int c = 0;

  initial begin
    int errors;

    $dumpfile("trace.vcd");
    $dumpvars;

    // Generate random input data
    for (int k = 0; k < K; k++) begin
      for (int c = 0; c < C; c++) begin
        mat_k[k][c] = WK'($urandom_range(-2**(WK-1), 2**(WK-1)-1));
      end
    end

    for (int k = 0; k < K; k++) begin
      for (int r = 0; r < R; r++) begin
        mat_x[k][r] = WX'($urandom_range(-2**(WX-1), 2**(WX-1)-1));
      end
    end

    // Generate reference output data
    for (int r = 0; r < R; r++) begin
      for (int c = 0; c < C; c++) begin
        for (int k = 0; k < K; k++) begin
          mat_y_ref[c][r] =
            $signed(mat_y_ref[c][r]) +
            $signed(mat_x[k][r]) * $signed(mat_k[k][c]);
        end
      end
    end

    // Print inputs and reference output for debugging
    $display("Matrix K(K,C):");
    for (int k = 0; k < K; k++) begin
      $write("  K[%0d]: ", k);
      for (int c = 0; c < C; c++) begin
        $write("%0d ", $signed(mat_k[k][c]));
      end
      $display("");
    end

    $display("Matrix X(K,R):");
    for (int k = 0; k < K; k++) begin
      $write("  X[%0d]: ", k);
      for (int r = 0; r < R; r++) begin
        $write("%0d ", $signed(mat_x[k][r]));
      end
      $display("");
    end

    $display("Matrix Y_ref(C,R):");
    for (int c = 0; c < C; c++) begin
      $write("  Y[%0d]: ", c);
      for (int r = 0; r < R; r++) begin
        $write("%0d ", $signed(mat_y_ref[c][r]));
      end
      $display("");
    end

    // Start simulation
    @(posedge clk);
    rstn <= 1;
    m_ready <= 1;
    repeat (2) @(posedge clk);

    // Send data to DUT using valid/ready handshake.
    // Only advance k when the DUT accepts the current input beat.
    for (int k = 0; k < K; k++) begin
      #1ps;
      s_valid <= 1;
      s_last  <= (k == K-1);
      sx_data <= mat_x[k];
      sk_data <= mat_k[k];

      do begin
        @(posedge clk);
      end while (!s_ready);
    end

    #1ps;
    s_valid <= 0;
    s_last  <= 0;

    // Receive output data using valid/ready handshake.
    while (1) begin
      @(posedge clk);
      #1ps;

      if (m_valid && m_ready) begin
        for (int r = 0; r < R; r++) begin
          mat_y_sim[c][r] = m_data[r];
        end

        c++;

        if (m_last) begin
          break;
        end
      end
    end

    @(posedge clk);

    // Print simulated output
    $display("Matrix Y_sim(C,R):");
    for (int c = 0; c < C; c++) begin
      $write("  Y[%0d]: ", c);
      for (int r = 0; r < R; r++) begin
        $write("%0d ", $signed(mat_y_sim[c][r]));
      end
      $display("");
    end

    // Check correctness
    errors = 0;

    for (int cc = 0; cc < C; cc++) begin
      for (int rr = 0; rr < R; rr++) begin
        if ($signed(mat_y_sim[cc][rr]) !== $signed(mat_y_ref[cc][rr])) begin
          $display(
            "ERROR: mismatch at c=%0d r=%0d: expected %0d, got %0d",
            cc,
            rr,
            $signed(mat_y_ref[cc][rr]),
            $signed(mat_y_sim[cc][rr])
          );
          errors++;
        end
      end
    end

    if (errors == 0) begin
      $display("SMOKE TEST PASSED");
    end else begin
      $fatal(1, "SMOKE TEST FAILED with %0d mismatches", errors);
    end

    $finish();
  end

endmodule