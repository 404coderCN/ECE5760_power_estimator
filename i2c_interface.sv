//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 04/24/2023 00:00:00 PM
// Design Name: 
// Module Name: i2c_master
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments: i2c_master for mcp3221
// 
//////////////////////////////////////////////////////////////////////////////////

module i2c_master (

    // basic signal
    input  logic clk,
    input  logic reset, // active high
    // control signals
    input  logic start,
    input  logic stop,
    // i2c ports
    output logic scl,
    inout  wire  sda,
    // To HPS
    output logic [11:0] data_out,
    output logic        data_valid
);

    //-----------------------------------------------------
    //  SIGNAL DEFINITION
    //-----------------------------------------------------
    localparam STATE_IDLE               = 4'd0;
    localparam STATE_START              = 4'd1;
    localparam STATE_ADDR               = 4'd2;
    localparam STATE_SLAVE_ACK          = 4'd3;
    localparam STATE_DATA_RECEIVE_UPPER = 4'd4;
    localparam STATE_MASTER_ACK_UPPER   = 4'd5;
    localparam STATE_DATA_RECEIVE_LOWER = 4'd6;
    localparam STATE_MASTER_ACK_LOWER   = 4'd7;
    localparam STATE_MASTER_NACK        = 4'd8;
    localparam STATE_STOP               = 4'd9;
    localparam STATE_WAIT               = 4'd10;

    logic [2:0] addr_cnt; //  counter for address
    logic [2:0] upper_cnt; // counter for upper 8-bit
    logic [2:0] lower_cnt; // counter for lower 8-bit

    //-----------------------------------------------------
    //  STATE UPDATE
    //-----------------------------------------------------
    logic [3:0] state_current;
    logic [3:0] state_next;

    always @( negedge clk ) begin
        if ( reset ) begin
            state_current <= STATE_IDLE;
        end
        else begin
            state_current <= state_next;
        end
    end

    //----------------------------------------------------------------------
    // STATE TRANSITION
    //----------------------------------------------------------------------
    always_comb begin
        case( state_current )
        
            STATE_IDLE: begin
                if ( start ) state_next = STATE_START;
                else state_next = STATE_IDLE;
            end
            STATE_START: begin
                state_next = STATE_ADDR;
            end
            STATE_ADDR: begin
                if ( addr_cnt == 3'd7 ) state_next = STATE_SLAVE_ACK; 
                else state_next = STATE_ADDR;
            end
            STATE_SLAVE_ACK: begin
                if ( !sda ) state_next = STATE_DATA_RECEIVE_UPPER;
                else state_next = STATE_IDLE;
            end
            STATE_DATA_RECEIVE_UPPER: begin
                if ( upper_cnt == 3'd7 ) state_next = STATE_MASTER_ACK_UPPER;
                else state_next = STATE_DATA_RECEIVE_UPPER;
            end
            STATE_MASTER_ACK_UPPER: begin
                if ( !sda ) state_next = STATE_DATA_RECEIVE_LOWER;
                else state_next = STATE_IDLE;
            end
            STATE_DATA_RECEIVE_LOWER: begin
                if ( lower_cnt == 3'd7 && !stop ) state_next = STATE_MASTER_ACK_LOWER;
                else if ( lower_cnt == 3'd7 && stop ) state_next = STATE_MASTER_NACK;
                else state_next = STATE_DATA_RECEIVE_LOWER;
            end
            STATE_MASTER_ACK_LOWER: begin
                state_next = STATE_DATA_RECEIVE_UPPER;
            end
            STATE_MASTER_NACK: begin
                state_next = STATE_STOP;
            end
            STATE_STOP: begin
                state_next = STATE_WAIT;
            end
            STATE_WAIT: begin
                state_next = STATE_IDLE;
            end

            default:
                state_next = STATE_IDLE;
                
        endcase         
    end

    //----------------------------------------------------------------------
    // STATE OUTPUT
    //----------------------------------------------------------------------
    always_comb begin // sck logic
        if ( ( state_current != STATE_IDLE ) && ( state_current != STATE_START )
         && ( state_current != STATE_STOP ) && ( state_current != STATE_WAIT ) ) scl = clk;
        else if ( state_current == STATE_STOP ) scl = 1'b0;
        else scl = 1'b1;
    end

    logic master_is_input; // tri-state logic for sda
    logic sda_temp;
    assign sda = master_is_input ? 1'bz : sda_temp; 

    always_comb begin
        case( state_current )
            STATE_IDLE: begin
                master_is_input = 1'b0;
            end
            STATE_START: begin
                master_is_input = 1'b0;
            end
            STATE_ADDR: begin
                master_is_input = 1'b0;
            end
            STATE_SLAVE_ACK: begin
                master_is_input = 1'b1;
            end
            STATE_DATA_RECEIVE_UPPER: begin
                master_is_input = 1'b1;
            end
            STATE_MASTER_ACK_UPPER: begin
                master_is_input = 1'b0;
            end
            STATE_DATA_RECEIVE_LOWER: begin
                master_is_input = 1'b1;
            end
            STATE_MASTER_ACK_LOWER: begin
                master_is_input = 1'b0;
            end
            STATE_MASTER_NACK: begin
                master_is_input = 1'b0;
            end 
            STATE_STOP: begin
                master_is_input = 1'b0;
            end
            STATE_WAIT: begin
                master_is_input = 1'b0;
            end
            default: begin
                master_is_input = 1'b1;
            end

        endcase
    end

    always_comb begin // master output for sda logic
        case( state_current )
            STATE_IDLE: begin
                sda_temp = 1'b1;
            end
            STATE_START: begin
                sda_temp = 1'b0;
            end
            STATE_ADDR: begin
                if ( addr_cnt == 0 ) sda_temp = 1'b1;
                else if ( addr_cnt == 1 ) sda_temp = 1'b0;
                else if ( addr_cnt == 2 ) sda_temp = 1'b0;
                else if ( addr_cnt == 3 ) sda_temp = 1'b1;
                else if ( addr_cnt == 4 ) sda_temp = 1'b1;
                else if ( addr_cnt == 5 ) sda_temp = 1'b0;
                else if ( addr_cnt == 6 ) sda_temp = 1'b1;
                else if ( addr_cnt == 7 ) sda_temp = 1'b1;
                else sda_temp = 1'b0;
            end
            STATE_MASTER_ACK_UPPER: begin
                sda_temp = 1'b0;
            end
            STATE_MASTER_ACK_LOWER: begin
                sda_temp = 1'b0;
            end
            STATE_MASTER_NACK: begin
                sda_temp = 1'b1;
            end 
            STATE_STOP: begin
                sda_temp = 1'b0;
            end
            STATE_WAIT: begin
                sda_temp = 1'b0;
            end
            default: begin
                sda_temp = 1'b0;
            end
        endcase
    end

    always_ff @( negedge clk ) begin // data_out store
        if ( ( state_current == STATE_DATA_RECEIVE_UPPER ) && ( upper_cnt <= 7 ) && ( upper_cnt >= 4 ) ) begin
            data_out <= { data_out[10:0], sda };
        end
        else if ( state_current == STATE_DATA_RECEIVE_LOWER ) begin
            data_out <= { data_out[10:0], sda };
        end
    end 

    assign data_valid = ( state_current == STATE_MASTER_ACK_LOWER ) || ( state_current == STATE_MASTER_NACK ); // data valid signal logic  

    //----------------------------------------------------------------------
    // CONTROL SIGNALS
    //----------------------------------------------------------------------
    
    always_ff @( negedge clk ) begin
        if ( state_current == STATE_ADDR ) addr_cnt <= addr_cnt + 1;
        else addr_cnt <= 3'd0;
    end

    always_ff @( negedge clk ) begin
        if ( state_current == STATE_DATA_RECEIVE_UPPER ) upper_cnt <= upper_cnt + 1;
        else upper_cnt <= 3'd0;
    end
    
    always_ff @( negedge clk ) begin
        if ( state_current == STATE_DATA_RECEIVE_LOWER ) lower_cnt <= lower_cnt + 1;
        else lower_cnt <= 3'd0;
    end

endmodule
