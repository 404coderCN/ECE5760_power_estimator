//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 04/24/2023 00:00:00 PM
// Design Name: 
// Module Name: segment decoder
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments: segment decoder
// 
//////////////////////////////////////////////////////////////////////////////////

module segment_decoder (
    input  logic               clk,
    //input  logic               current_val,
    input  logic signed [26:0] current,
    output logic         [6:0] hex0,
    output logic         [6:0] hex1,
    output logic         [6:0] hex2, // fraction
    output logic         [6:0] hex3, // integer
    output logic         [6:0] hex4  // sign
);

always_ff@ ( posedge clk ) begin
    if ( current[26] ) hex4 <= 7'b0111111;
    else hex4 <= 7'b1111111;
end

logic signed [4:0] current_int;
assign current_int = current[26:22];

always_ff@ ( posedge clk ) begin
    if ( !current[26] ) begin
        case( current_int )
            5'd1: begin
                hex3 <= 7'b1111001;
            end
            5'd2: begin
                hex3 <= 7'b0100100;
            end
            default: begin
                hex3 <= 7'b1000000;
            end
        endcase
    end
    else begin
        case( current_int )
            -5'd1: begin
                if (current[21:18]==4'd0) hex3 <= 7'b1111001;
                else hex3 <= 7'b1000000;
            end
            -5'd2: begin
                if (current[21:18]==4'd0) hex3 <= 7'b0100100;
                else hex3 <= 7'b1111001;
            end
            default: begin
                hex3 <= 7'b1000000;
            end
        endcase
    end
        
    
end

always_ff@ ( posedge clk ) begin
    case( current[21:18] )
        4'd0: begin
            hex2 <= 7'b1000000;
            hex1 <= 7'b1000000;
            hex0 <= 7'b1000000;
        end
        4'd1: begin
            hex2 <= current[26] ? 7'b0010000 : 7'b1000000;
            hex1 <= current[26] ? 7'b0110000 : 7'b0000010;
            hex0 <= current[26] ? 7'b1111000 : 7'b0110000;
        end
        4'd2: begin
            hex2 <= current[26] ? 7'b0000000 : 7'b1111001;
            hex1 <= current[26] ? 7'b1111000 : 7'b0100100;
            hex0 <= current[26] ? 7'b0010010 : 7'b0010010;
        end
        4'd3: begin
            hex2 <= current[26] ? 7'b0000000 : 7'b1111001;
            hex1 <= current[26] ? 7'b1111001 : 7'b0000000;
            hex0 <= current[26] ? 7'b0100100 : 7'b0000000;
        end
        4'd4: begin
            hex2 <= current[26] ? 7'b1111000 : 7'b0100100;
            hex1 <= current[26] ? 7'b0010010 : 7'b0010010; 
            hex0 <= current[26] ? 7'b1000000 : 7'b1000000;
        end
        4'd5: begin
            hex2 <= current[26] ? 7'b0000010 : 7'b0110000;
            hex1 <= current[26] ? 7'b0000000 : 7'b1111001;
            hex0 <= current[26] ? 7'b1111000 : 7'b0110000;
        end
        4'd6: begin
            hex2 <= current[26] ? 7'b0000010 : 7'b0110000;
            hex1 <= current[26] ? 7'b0100100 : 7'b1111000;
            hex0 <= current[26] ? 7'b0010010 : 7'b0010010;
        end
        4'd7: begin
            hex2 <= current[26] ? 7'b0010010 : 7'b0011001;
            hex1 <= current[26] ? 7'b0000010 : 7'b0110000; 
            hex0 <= current[26] ? 7'b0100100 : 7'b0000000;
        end
        4'd8: begin
            hex2 <= current[26] ? 7'b0010010 : 7'b0010010;
            hex1 <= current[26] ? 7'b1000000 : 7'b1000000;
            hex0 <= current[26] ? 7'b1000000 : 7'b1000000;
        end
        4'd9: begin
            hex2 <= current[26] ? 7'b0011001 : 7'b0010010;
            hex1 <= current[26] ? 7'b0110000 : 7'b0000010;
            hex0 <= current[26] ? 7'b1111000 : 7'b0110000;
        end
        4'd10: begin
            hex2 <= current[26] ? 7'b0110000 : 7'b0000010;
            hex1 <= current[26] ? 7'b1111000 : 7'b0100100;
            hex0 <= current[26] ? 7'b0010010 : 7'b0010010;
        end
        4'd11: begin
            hex2 <= current[26] ? 7'b0110000 : 7'b0000010;
            hex1 <= current[26] ? 7'b1111001 : 7'b0000000;
            hex0 <= current[26] ? 7'b0100100 : 7'b0000000;
        end
        4'd12: begin
            hex2 <= current[26] ? 7'b0100100 : 7'b1111000;
            hex1 <= current[26] ? 7'b0010010 : 7'b0010010;
            hex0 <= current[26] ? 7'b1000000 : 7'b1000000;
        end
        4'd13: begin
            hex2 <= current[26] ? 7'b1111001 : 7'b0000000;
            hex1 <= current[26] ? 7'b0000000 : 7'b1111001;
            hex0 <= current[26] ? 7'b1111000 : 7'b0110000;
        end
        4'd14: begin
            hex2 <= current[26] ? 7'b1111001 : 7'b0000000;
            hex1 <= current[26] ? 7'b0100100 : 7'b1111000;
            hex0 <= current[26] ? 7'b0010010 : 7'b0010010;
        end
        4'd15: begin
            hex2 <= current[26] ?  7'b1000000 : 7'b0010000;
            hex1 <= current[26] ?  7'b0000010 : 7'b0110000;
            hex0 <= current[26] ?  7'b0100100 : 7'b0000000;
        end
    endcase
end
    
endmodule