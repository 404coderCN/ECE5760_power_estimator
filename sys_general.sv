//-----------------------------------------------------
//  Multiplier
//-----------------------------------------------------
module signed_mult_27bit
(
    input  logic signed [26:0] a,
    input  logic signed [26:0] b,
    output logic signed [26:0] out
);

logic signed [53:0] mult_out;
assign mult_out = a * b;
assign out = {mult_out[53], mult_out[47:22]};

endmodule

module signed_mult_32bit
(
    input  logic signed [31:0] a,
    input  logic signed [31:0] b,
    output logic signed [31:0] out
);

logic signed [63:0] mult_out;
assign mult_out = a * b;
assign out = {mult_out[63], mult_out[53:23]};

endmodule


//-----------------------------------------------------
//  M10K Block
//-----------------------------------------------------

module M10K_512_18( 
    output reg [17:0] q,
    input      [17:0] d,
    input       [8:0] write_address, read_address, // PARAMETER!!!
    input             we, clk
);
	 // force M10K ram style
    reg [17:0] mem [511:0]  /* synthesis ramstyle = "no_rw_check, M10K" */; // PARAMETER!!!
	 
    always @ (posedge clk) begin
        if (we) begin
            mem[write_address] <= d;
        end
        q <= mem[read_address]; // q doesn't get d in this clock cycle
    end

endmodule

//-----------------------------------------------------
//  400kHz Clock
//-----------------------------------------------------

module clk_divider_400k(
	input  logic reset,
	input  logic clk_50M,
	output logic clk_400k
);
	logic [6:0]	counter;

	always_ff@( posedge clk_50M ) begin	
		if ( reset ) begin
			clk_400k <= 1'b1;
			counter <= 0;
		end
		else if ( counter == 124 ) begin
			clk_400k <= !clk_400k;
			counter <= 0;
		end
		else begin
			counter <= counter + 1;
		end
	end
endmodule

//-----------------------------------------------------
//  10Hz Clock
//-----------------------------------------------------

module clk_divider_10(
	input  logic reset,
	input  logic clk_50M,
	output logic clk_10
);
	logic [22:0]	counter;

	always_ff@( posedge clk_50M ) begin	
		if ( reset ) begin
			clk_10 <= 1'b1;
			counter <= 0;
		end
		else if ( counter == 4999999 ) begin
			clk_10 <= !clk_10;
			counter <= 0;
		end
		else begin
			counter <= counter + 1;
		end
	end
endmodule

//-----------------------------------------------------
//  Integrater
//-----------------------------------------------------
module integrater(
	// basic
    input logic         reset,
    input logic         clk,
    // control: signals to start and clear integration
    input logic         data_valid,  // = data_valid from i2c interface
	input logic         start,
	input logic         stop,
	input logic         cali,
    // From I2C interface
    input logic signed [26:0] input_data, // 5.22
    // To HPS 
    output logic signed [63:0] energy_out,
	output logic        [31:0] cycle_cnt
);

    // PARAMETER DEFINITION 
    logic signed [31:0] unit_voltage = 32'd9059; // (2^23)*18*12*1/200000

	logic signed [31:0] input_data_32bit; // 9.23
	assign input_data_32bit = { {4{input_data[26]}}, input_data, 1'b0};

	logic signed [31:0]	unit_energy;

	// UNIT ENERGY
	signed_mult_32bit multiply(
		.a(unit_voltage),
		.b(input_data_32bit),
		.out(unit_energy)
	);

	// CONTROL
	logic out_ind;
	always_ff @( posedge clk or posedge reset or posedge cali ) begin
		if ( reset || cali ) out_ind <= 1'b0;
		else if ( stop ) out_ind <= 1'b1;
		else if ( start ) out_ind <= 1'b0;
	end

	// ENERGY COMPUTATION
	always_ff @( posedge clk or posedge reset or posedge cali ) begin
		if ( reset || cali ) energy_out <= 32'd0;
		else if ( !out_ind && data_valid ) energy_out <= energy_out + { {32{unit_energy[31]}}, unit_energy}; 
	end

	// TIME COUNTER
	always_ff @( posedge clk or posedge reset or posedge cali ) begin
		if ( reset || cali ) cycle_cnt <= 32'd0;
		else if ( !out_ind && data_valid ) cycle_cnt <= cycle_cnt + 1;
	end

	// logic signed [31:0] accum_reg;
    // logic signed [31:0] accum_mux;
    // logic signed [31:0] accum_temp;

    // always_ff@ ( posedge clk ) begin
    //     if ( reset ) accum_temp <= 32'd0;
    //     else if(start) accum_temp <= accum_temp + unit_power;
	// 	else accum_temp <= accum_temp;
    // end

    // assign accum_mux = (reset) ? 32'b0 : accum_temp;
    // assign accum_reg = (reset) ? 32'b0 : accum_mux;

	// assign data_out = accum_reg;

endmodule