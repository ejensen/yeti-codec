// 2048 entry table for reciprocal multiplies in the range decoder.
// x*reciprocal_table[y]>>32 will be equal to (x/y) or (x/y-1) 
// for all x <= 0x80000000 and all 0<y<2048.
// Values of x/y for y >= 2048 can be approximated using this table
// with acceptable accuracy for the range decoder
const unsigned int reciprocal_table[] = { 
	0, 0xFFFFFFFF, 0x80000000, 0x55555555, 0x40000000, 0x33333333, 0x2AAAAAAA, 0x24924924, 0x20000000,
	0x1C71C71C, 0x19999999, 0x1745D174, 0x15555555, 0x13B13B13, 0x12492492, 0x11111111, 0x10000000,
	0xF0F0F0F, 0xE38E38E, 0xD79435E, 0xCCCCCCC, 0xC30C30C, 0xBA2E8BA, 0xB21642C, 0xAAAAAAA,
	0xA3D70A3, 0x9D89D89, 0x97B425E, 0x9249249, 0x8D3DCB0, 0x8888888, 0x8421084, 0x8000000,
	0x7C1F07C, 0x7878787, 0x7507507, 0x71C71C7, 0x6EB3E45, 0x6BCA1AF, 0x6906906, 0x6666666,
	0x63E7063, 0x6186186, 0x5F417D0, 0x5D1745D, 0x5B05B05, 0x590B216, 0x572620A, 0x5555555,
	0x5397829, 0x51EB851, 0x5050505, 0x4EC4EC4, 0x4D4873E, 0x4BDA12F, 0x4A7904A, 0x4924924,
	0x47DC11F, 0x469EE58, 0x456C797, 0x4444444, 0x4325C53, 0x4210842, 0x4104104, 0x4000000,
	0x3F03F03, 0x3E0F83E, 0x3D22635, 0x3C3C3C3, 0x3B5CC0E, 0x3A83A83, 0x39B0AD1, 0x38E38E3,
	0x381C0E0, 0x3759F22, 0x369D036, 0x35E50D7, 0x3531DEC, 0x3483483, 0x33D91D2, 0x3333333,
	0x329161F, 0x31F3831, 0x3159721, 0x30C30C3, 0x3030303, 0x2FA0BE8, 0x2F14990, 0x2E8BA2E,
	0x2E05C0B, 0x2D82D82, 0x2D02D02, 0x2C8590B, 0x2C0B02C, 0x2B93105, 0x2B1DA46, 0x2AAAAAA,
	0x2A3A0FD, 0x29CBC14, 0x295FAD4, 0x28F5C28, 0x288DF0C, 0x2828282, 0x27C4597, 0x2762762,
	0x2702702, 0x26A439F, 0x2647C69, 0x25ED097, 0x2593F69, 0x253C825, 0x24E6A17, 0x2492492,
	0x243F6F0, 0x23EE08F, 0x239E0D5, 0x234F72C, 0x2302302, 0x22B63CB, 0x226B902, 0x2222222,
	0x21D9EAD, 0x2192E29, 0x214D021, 0x2108421, 0x20C49BA, 0x2082082, 0x2040810, 0x2000000,
	0x1FC07F0, 0x1F81F81, 0x1F44659, 0x1F07C1F, 0x1ECC07B, 0x1E9131A, 0x1E573AC, 0x1E1E1E1,
	0x1DE5D6E, 0x1DAE607, 0x1D77B65, 0x1D41D41, 0x1D0CB58, 0x1CD8568, 0x1CA4B30, 0x1C71C71,
	0x1C3F8F0, 0x1C0E070, 0x1BDD2B8, 0x1BACF91, 0x1B7D6C3, 0x1B4E81B, 0x1B20364, 0x1AF286B,
	0x1AC5701, 0x1A98EF6, 0x1A6D01A, 0x1A41A41, 0x1A16D3F, 0x19EC8E9, 0x19C2D14, 0x1999999,
	0x1970E4F, 0x1948B0F, 0x1920FB4, 0x18F9C18, 0x18D3018, 0x18ACB90, 0x1886E5F, 0x1861861,
	0x183C977, 0x1818181, 0x17F405F, 0x17D05F4, 0x17AD220, 0x178A4C8, 0x1767DCE, 0x1745D17,
	0x1724287, 0x1702E05, 0x16E1F76, 0x16C16C1, 0x16A13CD, 0x1681681, 0x1661EC6, 0x1642C85,
	0x1623FA7, 0x1605816, 0x15E75BB, 0x15C9882, 0x15AC056, 0x158ED23, 0x1571ED3, 0x1555555,
	0x1539094, 0x151D07E, 0x1501501, 0x14E5E0A, 0x14CAB88, 0x14AFD6A, 0x149539E, 0x147AE14,
	0x1460CBC, 0x1446F86, 0x142D662, 0x1414141, 0x13FB013, 0x13E22CB, 0x13C995A, 0x13B13B1,
	0x13991C2, 0x1381381, 0x13698DF, 0x13521CF, 0x133AE45, 0x1323E34, 0x130D190, 0x12F684B,
	0x12E025C, 0x12C9FB4, 0x12B404A, 0x129E412, 0x1288B01, 0x127350B, 0x125E227, 0x1249249,
	0x1234567, 0x121FB78, 0x120B470, 0x11F7047, 0x11E2EF3, 0x11CF06A, 0x11BB4A4, 0x11A7B96,
	0x1194538, 0x1181181, 0x116E068, 0x115B1E5, 0x11485F0, 0x1135C81, 0x112358E, 0x1111111,
	0x10FEF01, 0x10ECF56, 0x10DB20A, 0x10C9714, 0x10B7E6E, 0x10A6810, 0x10953F3, 0x1084210,
	0x1073260, 0x10624DD, 0x105197F, 0x1041041, 0x103091B, 0x1020408, 0x1010101, 0x1000000,
	0xFF00FF, 0xFE03F8, 0xFD08E5, 0xFC0FC0, 0xFB1885, 0xFA232C, 0xF92FB2, 0xF83E0F,
	0xF74E3F, 0xF6603D, 0xF57403, 0xF4898D, 0xF3A0D5, 0xF2B9D6, 0xF1D48B, 0xF0F0F0,
	0xF00F00, 0xEF2EB7, 0xEE500E, 0xED7303, 0xEC9791, 0xEBBDB2, 0xEAE564, 0xEA0EA0,
	0xE93965, 0xE865AC, 0xE79372, 0xE6C2B4, 0xE5F36C, 0xE52598, 0xE45932, 0xE38E38,
	0xE2C4A6, 0xE1FC78, 0xE135A9, 0xE07038, 0xDFAC1F, 0xDEE95C, 0xDE27EB, 0xDD67C8,
	0xDCA8F1, 0xDBEB61, 0xDB2F17, 0xDA740D, 0xD9BA42, 0xD901B2, 0xD84A59, 0xD79435,
	0xD6DF43, 0xD62B80, 0xD578E9, 0xD4C77B, 0xD41732, 0xD3680D, 0xD2BA08, 0xD20D20,
	0xD16154, 0xD0B69F, 0xD00D00, 0xCF6474, 0xCEBCF8, 0xCE168A, 0xCD7127, 0xCCCCCC,
	0xCC2978, 0xCB8727, 0xCAE5D8, 0xCA4587, 0xC9A633, 0xC907DA, 0xC86A78, 0xC7CE0C,
	0xC73293, 0xC6980C, 0xC5FE74, 0xC565C8, 0xC4CE07, 0xC4372F, 0xC3A13D, 0xC30C30,
	0xC27806, 0xC1E4BB, 0xC15250, 0xC0C0C0, 0xC0300C, 0xBFA02F, 0xBF112A, 0xBE82FA,
	0xBDF59C, 0xBD6910, 0xBCDD53, 0xBC5264, 0xBBC840, 0xBB3EE7, 0xBAB656, 0xBA2E8B,
	0xB9A786, 0xB92143, 0xB89BC3, 0xB81702, 0xB79300, 0xB70FBB, 0xB68D31, 0xB60B60,
	0xB58A48, 0xB509E6, 0xB48A39, 0xB40B40, 0xB38CF9, 0xB30F63, 0xB2927C, 0xB21642,
	0xB19AB5, 0xB11FD3, 0xB0A59B, 0xB02C0B, 0xAFB321, 0xAF3ADD, 0xAEC33E, 0xAE4C41,
	0xADD5E6, 0xAD602B, 0xACEB0F, 0xAC7691, 0xAC02B0, 0xAB8F69, 0xAB1CBD, 0xAAAAAA,
	0xAA392F, 0xA9C84A, 0xA957FA, 0xA8E83F, 0xA87917, 0xA80A80, 0xA79C7B, 0xA72F05,
	0xA6C21D, 0xA655C4, 0xA5E9F6, 0xA57EB5, 0xA513FD, 0xA4A9CF, 0xA44029, 0xA3D70A,
	0xA36E71, 0xA3065E, 0xA29ECF, 0xA237C3, 0xA1D139, 0xA16B31, 0xA105A9, 0xA0A0A0,
	0xA03C16, 0x9FD809, 0x9F747A, 0x9F1165, 0x9EAECC, 0x9E4CAD, 0x9DEB06, 0x9D89D8,
	0x9D2921, 0x9CC8E1, 0x9C6916, 0x9C09C0, 0x9BAADE, 0x9B4C6F, 0x9AEE72, 0x9A90E7,
	0x9A33CD, 0x99D722, 0x997AE7, 0x991F1A, 0x98C3BA, 0x9868C8, 0x980E41, 0x97B425,
	0x975A75, 0x97012E, 0x96A850, 0x964FDA, 0x95F7CC, 0x95A025, 0x9548E4, 0x94F209,
	0x949B92, 0x944580, 0x93EFD1, 0x939A85, 0x93459B, 0x92F113, 0x929CEB, 0x924924,
	0x91F5BC, 0x91A2B3, 0x915009, 0x90FDBC, 0x90ABCC, 0x905A38, 0x900900, 0x8FB823,
	0x8F67A1, 0x8F1779, 0x8EC7AB, 0x8E7835, 0x8E2917, 0x8DDA52, 0x8D8BE3, 0x8D3DCB,
	0x8CF008, 0x8CA29C, 0x8C5584, 0x8C08C0, 0x8BBC50, 0x8B7034, 0x8B246A, 0x8AD8F2,
	0x8A8DCD, 0x8A42F8, 0x89F874, 0x89AE40, 0x89645C, 0x891AC7, 0x88D180, 0x888888,
	0x883FDD, 0x87F780, 0x87AF6F, 0x8767AB, 0x872032, 0x86D905, 0x869222, 0x864B8A,
	0x86053C, 0x85BF37, 0x85797B, 0x853408, 0x84EEDD, 0x84A9F9, 0x84655D, 0x842108,
	0x83DCF9, 0x839930, 0x8355AC, 0x83126E, 0x82CF75, 0x828CBF, 0x824A4E, 0x820820,
	0x81C635, 0x81848D, 0x814327, 0x810204, 0x80C121, 0x808080, 0x804020, 0x800000,
	0x7FC01F, 0x7F807F, 0x7F411E, 0x7F01FC, 0x7EC318, 0x7E8472, 0x7E460A, 0x7E07E0,
	0x7DC9F3, 0x7D8C42, 0x7D4ECE, 0x7D1196, 0x7CD49A, 0x7C97D9, 0x7C5B53, 0x7C1F07,
	0x7BE2F6, 0x7BA71F, 0x7B6B82, 0x7B301E, 0x7AF4F3, 0x7ABA01, 0x7A7F48, 0x7A44C6,
	0x7A0A7C, 0x79D06A, 0x79968F, 0x795CEB, 0x79237D, 0x78EA45, 0x78B144, 0x787878,
	0x783FE1, 0x780780, 0x77CF53, 0x77975B, 0x775F97, 0x772807, 0x76F0AA, 0x76B981,
	0x76828B, 0x764BC8, 0x761537, 0x75DED9, 0x75A8AC, 0x7572B2, 0x753CE8, 0x750750,
	0x74D1E9, 0x749CB2, 0x7467AC, 0x7432D6, 0x73FE30, 0x73C9B9, 0x739572, 0x73615A,
	0x732D70, 0x72F9B6, 0x72C62A, 0x7292CC, 0x725F9B, 0x722C99, 0x71F9C4, 0x71C71C,
	0x7194A1, 0x716253, 0x713031, 0x70FE3C, 0x70CC72, 0x709AD4, 0x706962, 0x70381C,
	0x700700, 0x6FD60F, 0x6FA549, 0x6F74AE, 0x6F443C, 0x6F13F5, 0x6EE3D8, 0x6EB3E4,
	0x6E8419, 0x6E5478, 0x6E2500, 0x6DF5B0, 0x6DC68A, 0x6D978B, 0x6D68B5, 0x6D3A06,
	0x6D0B80, 0x6CDD21, 0x6CAEE9, 0x6C80D9, 0x6C52EF, 0x6C252C, 0x6BF790, 0x6BCA1A,
	0x6B9CCB, 0x6B6FA1, 0x6B429E, 0x6B15C0, 0x6AE907, 0x6ABC74, 0x6A9006, 0x6A63BD,
	0x6A3799, 0x6A0B99, 0x69DFBD, 0x69B406, 0x698873, 0x695D04, 0x6931B8, 0x690690,
	0x68DB8B, 0x68B0AA, 0x6885EB, 0x685B4F, 0x6830D6, 0x680680, 0x67DC4C, 0x67B23A,
	0x67884A, 0x675E7C, 0x6734D0, 0x670B45, 0x66E1DB, 0x66B893, 0x668F6C, 0x666666,
	0x663D80, 0x6614BC, 0x65EC17, 0x65C393, 0x659B30, 0x6572EC, 0x654AC8, 0x6522C3,
	0x64FADF, 0x64D319, 0x64AB74, 0x6483ED, 0x645C85, 0x64353C, 0x640E11, 0x63E706,
	0x63C018, 0x639949, 0x637299, 0x634C06, 0x632591, 0x62FF3A, 0x62D900, 0x62B2E4,
	0x628CE5, 0x626703, 0x62413F, 0x621B97, 0x61F60D, 0x61D09E, 0x61AB4D, 0x618618,
	0x6160FF, 0x613C03, 0x611722, 0x60F25D, 0x60CDB5, 0x60A928, 0x6084B6, 0x606060,
	0x603C25, 0x601806, 0x5FF401, 0x5FD017, 0x5FAC49, 0x5F8895, 0x5F64FB, 0x5F417D,
	0x5F1E18, 0x5EFACE, 0x5ED79E, 0x5EB488, 0x5E918C, 0x5E6EA9, 0x5E4BE1, 0x5E2932,
	0x5E069C, 0x5DE420, 0x5DC1BD, 0x5D9F73, 0x5D7D42, 0x5D5B2B, 0x5D392C, 0x5D1745,
	0x5CF578, 0x5CD3C3, 0x5CB226, 0x5C90A1, 0x5C6F35, 0x5C4DE1, 0x5C2CA5, 0x5C0B81,
	0x5BEA75, 0x5BC980, 0x5BA8A3, 0x5B87DD, 0x5B672F, 0x5B4698, 0x5B2618, 0x5B05B0,
	0x5AE55E, 0x5AC524, 0x5AA500, 0x5A84F3, 0x5A64FC, 0x5A451C, 0x5A2553, 0x5A05A0,
	0x59E603, 0x59C67C, 0x59A70C, 0x5987B1, 0x59686C, 0x59493E, 0x592A24, 0x590B21,
	0x58EC33, 0x58CD5A, 0x58AE97, 0x588FE9, 0x587151, 0x5852CD, 0x58345F, 0x581605,
	0x57F7C0, 0x57D990, 0x57BB75, 0x579D6E, 0x577F7C, 0x57619F, 0x5743D5, 0x572620,
	0x57087F, 0x56EAF3, 0x56CD7A, 0x56B015, 0x5692C4, 0x567587, 0x56585E, 0x563B48,
	0x561E46, 0x560158, 0x55E47C, 0x55C7B4, 0x55AB00, 0x558E5E, 0x5571D0, 0x555555,
	0x5538ED, 0x551C97, 0x550055, 0x54E425, 0x54C807, 0x54ABFD, 0x549005, 0x54741F,
	0x54584C, 0x543C8B, 0x5420DC, 0x540540, 0x53E9B5, 0x53CE3D, 0x53B2D7, 0x539782,
	0x537C3F, 0x53610E, 0x5345EF, 0x532AE2, 0x530FE6, 0x52F4FB, 0x52DA22, 0x52BF5A,
	0x52A4A3, 0x5289FE, 0x526F6A, 0x5254E7, 0x523A75, 0x522014, 0x5205C4, 0x51EB85,
	0x51D156, 0x51B738, 0x519D2B, 0x51832F, 0x516943, 0x514F67, 0x51359C, 0x511BE1,
	0x510237, 0x50E89C, 0x50CF12, 0x50B598, 0x509C2E, 0x5082D4, 0x50698A, 0x505050,
	0x503725, 0x501E0B, 0x500500, 0x4FEC04, 0x4FD319, 0x4FBA3D, 0x4FA170, 0x4F88B2,
	0x4F7004, 0x4F5766, 0x4F3ED6, 0x4F2656, 0x4F0DE5, 0x4EF583, 0x4EDD30, 0x4EC4EC,
	0x4EACB7, 0x4E9490, 0x4E7C79, 0x4E6470, 0x4E4C76, 0x4E348B, 0x4E1CAE, 0x4E04E0,
	0x4DED20, 0x4DD56F, 0x4DBDCC, 0x4DA637, 0x4D8EB1, 0x4D7739, 0x4D5FCF, 0x4D4873,
	0x4D3126, 0x4D19E6, 0x4D02B5, 0x4CEB91, 0x4CD47B, 0x4CBD73, 0x4CA679, 0x4C8F8D,
	0x4C78AE, 0x4C61DD, 0x4C4B19, 0x4C3464, 0x4C1DBB, 0x4C0720, 0x4BF093, 0x4BDA12,
	0x4BC3A0, 0x4BAD3A, 0x4B96E2, 0x4B8097, 0x4B6A58, 0x4B5428, 0x4B3E04, 0x4B27ED,
	0x4B11E3, 0x4AFBE6, 0x4AE5F6, 0x4AD012, 0x4ABA3C, 0x4AA472, 0x4A8EB5, 0x4A7904,
	0x4A6360, 0x4A4DC9, 0x4A383E, 0x4A22C0, 0x4A0D4E, 0x49F7E8, 0x49E28F, 0x49CD42,
	0x49B802, 0x49A2CD, 0x498DA5, 0x497889, 0x496379, 0x494E75, 0x49397E, 0x492492,
	0x490FB2, 0x48FADE, 0x48E616, 0x48D159, 0x48BCA9, 0x48A804, 0x48936B, 0x487EDE,
	0x486A5C, 0x4855E6, 0x48417B, 0x482D1C, 0x4818C8, 0x480480, 0x47F043, 0x47DC11,
	0x47C7EB, 0x47B3D0, 0x479FC1, 0x478BBC, 0x4777C3, 0x4763D5, 0x474FF2, 0x473C1A,
	0x47284D, 0x47148B, 0x4700D5, 0x46ED29, 0x46D987, 0x46C5F1, 0x46B266, 0x469EE5,
	0x468B6F, 0x467804, 0x4664A3, 0x46514E, 0x463E02, 0x462AC2, 0x46178B, 0x460460,
	0x45F13F, 0x45DE28, 0x45CB1C, 0x45B81A, 0x45A522, 0x459235, 0x457F52, 0x456C79,
	0x4559AA, 0x4546E6, 0x45342C, 0x45217C, 0x450ED6, 0x44FC3A, 0x44E9A8, 0x44D720,
	0x44C4A2, 0x44B22E, 0x449FC3, 0x448D63, 0x447B0D, 0x4468C0, 0x44567D, 0x444444,
	0x443214, 0x441FEE, 0x440DD2, 0x43FBC0, 0x43E9B7, 0x43D7B7, 0x43C5C2, 0x43B3D5,
	0x43A1F2, 0x439019, 0x437E49, 0x436C82, 0x435AC5, 0x434911, 0x433766, 0x4325C5,
	0x43142D, 0x43029E, 0x42F118, 0x42DF9B, 0x42CE28, 0x42BCBD, 0x42AB5C, 0x429A04,
	0x4288B4, 0x42776E, 0x426631, 0x4254FC, 0x4243D1, 0x4232AE, 0x422195, 0x421084,
	0x41FF7C, 0x41EE7C, 0x41DD86, 0x41CC98, 0x41BBB2, 0x41AAD6, 0x419A02, 0x418937,
	0x417874, 0x4167BA, 0x415708, 0x41465F, 0x4135BF, 0x412527, 0x411497, 0x410410,
	0x40F391, 0x40E31A, 0x40D2AC, 0x40C246, 0x40B1E9, 0x40A193, 0x409146, 0x408102,
	0x4070C5, 0x406090, 0x405064, 0x404040, 0x403024, 0x402010, 0x401004, 0x400000,
	0x3FF003, 0x3FE00F, 0x3FD023, 0x3FC03F, 0x3FB063, 0x3FA08F, 0x3F90C2, 0x3F80FE,
	0x3F7141, 0x3F618C, 0x3F51DE, 0x3F4239, 0x3F329B, 0x3F2305, 0x3F1377, 0x3F03F0,
	0x3EF471, 0x3EE4F9, 0x3ED589, 0x3EC621, 0x3EB6C0, 0x3EA767, 0x3E9815, 0x3E88CB,
	0x3E7988, 0x3E6A4D, 0x3E5B19, 0x3E4BEC, 0x3E3CC7, 0x3E2DA9, 0x3E1E93, 0x3E0F83,
	0x3E007C, 0x3DF17B, 0x3DE282, 0x3DD38F, 0x3DC4A5, 0x3DB5C1, 0x3DA6E4, 0x3D980F,
	0x3D8941, 0x3D7A79, 0x3D6BB9, 0x3D5D00, 0x3D4E4F, 0x3D3FA4, 0x3D3100, 0x3D2263,
	0x3D13CD, 0x3D053E, 0x3CF6B6, 0x3CE835, 0x3CD9BB, 0x3CCB47, 0x3CBCDB, 0x3CAE75,
	0x3CA016, 0x3C91BE, 0x3C836D, 0x3C7522, 0x3C66DF, 0x3C58A2, 0x3C4A6B, 0x3C3C3C,
	0x3C2E13, 0x3C1FF0, 0x3C11D5, 0x3C03C0, 0x3BF5B1, 0x3BE7A9, 0x3BD9A8, 0x3BCBAD,
	0x3BBDB9, 0x3BAFCB, 0x3BA1E4, 0x3B9403, 0x3B8629, 0x3B7855, 0x3B6A88, 0x3B5CC0,
	0x3B4F00, 0x3B4145, 0x3B3391, 0x3B25E4, 0x3B183C, 0x3B0A9B, 0x3AFD01, 0x3AEF6C,
	0x3AE1DE, 0x3AD456, 0x3AC6D4, 0x3AB959, 0x3AABE3, 0x3A9E74, 0x3A910B, 0x3A83A8,
	0x3A764B, 0x3A68F4, 0x3A5BA3, 0x3A4E59, 0x3A4114, 0x3A33D6, 0x3A269D, 0x3A196B,
	0x3A0C3E, 0x39FF18, 0x39F1F7, 0x39E4DC, 0x39D7C7, 0x39CAB9, 0x39BDB0, 0x39B0AD,
	0x39A3AF, 0x3996B8, 0x3989C6, 0x397CDB, 0x396FF5, 0x396315, 0x39563A, 0x394966,
	0x393C97, 0x392FCD, 0x39230A, 0x39164C, 0x390994, 0x38FCE2, 0x38F035, 0x38E38E,
	0x38D6EC, 0x38CA50, 0x38BDBA, 0x38B129, 0x38A49E, 0x389818, 0x388B98, 0x387F1E,
	0x3872A8, 0x386639, 0x3859CF, 0x384D6A, 0x38410B, 0x3834B1, 0x38285D, 0x381C0E,
	0x380FC4, 0x380380, 0x37F741, 0x37EB07, 0x37DED3, 0x37D2A4, 0x37C67B, 0x37BA57,
	0x37AE38, 0x37A21E, 0x379609, 0x3789FA, 0x377DF0, 0x3771EC, 0x3765EC, 0x3759F2,
	0x374DFC, 0x37420C, 0x373622, 0x372A3C, 0x371E5B, 0x371280, 0x3706A9, 0x36FAD8,
	0x36EF0C, 0x36E345, 0x36D782, 0x36CBC5, 0x36C00D, 0x36B45A, 0x36A8AC, 0x369D03,
	0x36915F, 0x3685C0, 0x367A25, 0x366E90, 0x366300, 0x365774, 0x364BEE, 0x36406C,
	0x3634EF, 0x362977, 0x361E04, 0x361296, 0x36072C, 0x35FBC8, 0x35F068, 0x35E50D,
	0x35D9B7, 0x35CE65, 0x35C318, 0x35B7D0, 0x35AC8D, 0x35A14F, 0x359615, 0x358AE0,
	0x357FAF, 0x357483, 0x35695C, 0x355E3A, 0x35531C, 0x354803, 0x353CEE, 0x3531DE,
	0x3526D3, 0x351BCC, 0x3510CA, 0x3505CC, 0x34FAD3, 0x34EFDE, 0x34E4EE, 0x34DA03,
	0x34CF1C, 0x34C439, 0x34B95B, 0x34AE82, 0x34A3AC, 0x3498DC, 0x348E10, 0x348348,
	0x347884, 0x346DC5, 0x34630B, 0x345855, 0x344DA3, 0x3442F5, 0x34384C, 0x342DA7,
	0x342307, 0x34186B, 0x340DD3, 0x340340, 0x33F8B1, 0x33EE26, 0x33E39F, 0x33D91D,
	0x33CE9F, 0x33C425, 0x33B9AF, 0x33AF3E, 0x33A4D0, 0x339A68, 0x339003, 0x3385A2,
	0x337B46, 0x3370ED, 0x336699, 0x335C49, 0x3351FD, 0x3347B6, 0x333D72, 0x333333,
	0x3328F7, 0x331EC0, 0x33148D, 0x330A5E, 0x330033, 0x32F60B, 0x32EBE8, 0x32E1C9,
	0x32D7AE, 0x32CD98, 0x32C385, 0x32B976, 0x32AF6B, 0x32A564, 0x329B61, 0x329161,
	0x328766, 0x327D6F, 0x32737C, 0x32698C, 0x325FA1, 0x3255BA, 0x324BD6, 0x3241F6,
	0x32381A, 0x322E42, 0x32246E, 0x321A9E, 0x3210D1, 0x320708, 0x31FD44, 0x31F383,
	0x31E9C5, 0x31E00C, 0x31D656, 0x31CCA4, 0x31C2F6, 0x31B94C, 0x31AFA5, 0x31A603,
	0x319C63, 0x3192C8, 0x318930, 0x317F9D, 0x31760C, 0x316C80, 0x3162F7, 0x315972,
	0x314FF0, 0x314672, 0x313CF8, 0x313381, 0x312A0E, 0x31209F, 0x311733, 0x310DCB,
	0x310467, 0x30FB06, 0x30F1A9, 0x30E84F, 0x30DEF9, 0x30D5A6, 0x30CC57, 0x30C30C,
	0x30B9C4, 0x30B07F, 0x30A73E, 0x309E01, 0x3094C7, 0x308B91, 0x30825E, 0x30792E,
	0x307003, 0x3066DA, 0x305DB5, 0x305494, 0x304B75, 0x30425B, 0x303944, 0x303030,
	0x30271F, 0x301E12, 0x301509, 0x300C03, 0x300300, 0x2FFA00, 0x2FF104, 0x2FE80B,
	0x2FDF16, 0x2FD624, 0x2FCD35, 0x2FC44A, 0x2FBB62, 0x2FB27D, 0x2FA99C, 0x2FA0BE,
	0x2F97E3, 0x2F8F0C, 0x2F8638, 0x2F7D67, 0x2F7499, 0x2F6BCF, 0x2F6307, 0x2F5A44,
	0x2F5183, 0x2F48C6, 0x2F400B, 0x2F3754, 0x2F2EA1, 0x2F25F0, 0x2F1D43, 0x2F1499,
	0x2F0BF2, 0x2F034E, 0x2EFAAD, 0x2EF210, 0x2EE975, 0x2EE0DE, 0x2ED84A, 0x2ECFB9,
	0x2EC72C, 0x2EBEA1, 0x2EB619, 0x2EAD95, 0x2EA514, 0x2E9C96, 0x2E941A, 0x2E8BA2,
	0x2E832D, 0x2E7ABC, 0x2E724D, 0x2E69E1, 0x2E6178, 0x2E5913, 0x2E50B0, 0x2E4850,
	0x2E3FF4, 0x2E379A, 0x2E2F44, 0x2E26F0, 0x2E1EA0, 0x2E1652, 0x2E0E08, 0x2E05C0,
	0x2DFD7C, 0x2DF53A, 0x2DECFB, 0x2DE4C0, 0x2DDC87, 0x2DD451, 0x2DCC1E, 0x2DC3EE,
	0x2DBBC1, 0x2DB397, 0x2DAB70, 0x2DA34C, 0x2D9B2A, 0x2D930C, 0x2D8AF0, 0x2D82D8,
	0x2D7AC2, 0x2D72AF, 0x2D6A9F, 0x2D6292, 0x2D5A87, 0x2D5280, 0x2D4A7B, 0x2D4279,
	0x2D3A7A, 0x2D327E, 0x2D2A85, 0x2D228E, 0x2D1A9A, 0x2D12A9, 0x2D0ABB, 0x2D02D0,
	0x2CFAE7, 0x2CF301, 0x2CEB1E, 0x2CE33E, 0x2CDB60, 0x2CD386, 0x2CCBAE, 0x2CC3D8,
	0x2CBC06, 0x2CB436, 0x2CAC69, 0x2CA49F, 0x2C9CD7, 0x2C9512, 0x2C8D50, 0x2C8590,
	0x2C7DD3, 0x2C7619, 0x2C6E62, 0x2C66AD, 0x2C5EFB, 0x2C574B, 0x2C4F9F, 0x2C47F4,
	0x2C404D, 0x2C38A8, 0x2C3106, 0x2C2966, 0x2C21C9, 0x2C1A2F, 0x2C1297, 0x2C0B02,
	0x2C0370, 0x2BFBE0, 0x2BF453, 0x2BECC8, 0x2BE540, 0x2BDDBA, 0x2BD637, 0x2BCEB7,
	0x2BC739, 0x2BBFBE, 0x2BB845, 0x2BB0CF, 0x2BA95B, 0x2BA1EA, 0x2B9A7C, 0x2B9310,
	0x2B8BA6, 0x2B843F, 0x2B7CDB, 0x2B7579, 0x2B6E1A, 0x2B66BD, 0x2B5F62, 0x2B580A,
	0x2B50B5, 0x2B4962, 0x2B4211, 0x2B3AC3, 0x2B3378, 0x2B2C2F, 0x2B24E8, 0x2B1DA4,
	0x2B1662, 0x2B0F23, 0x2B07E6, 0x2B00AC, 0x2AF973, 0x2AF23E, 0x2AEB0B, 0x2AE3DA,
	0x2ADCAC, 0x2AD580, 0x2ACE56, 0x2AC72F, 0x2AC00A, 0x2AB8E8, 0x2AB1C8, 0x2AAAAA,
	0x2AA38F, 0x2A9C76, 0x2A955F, 0x2A8E4B, 0x2A8739, 0x2A802A, 0x2A791D, 0x2A7212,
	0x2A6B0A, 0x2A6403, 0x2A5D00, 0x2A55FE, 0x2A4EFF, 0x2A4802, 0x2A4108, 0x2A3A0F,
	0x2A3319, 0x2A2C26, 0x2A2534, 0x2A1E45, 0x2A1758, 0x2A106E, 0x2A0986, 0x2A02A0,
	0x29FBBC, 0x29F4DA, 0x29EDFB, 0x29E71E, 0x29E044, 0x29D96B, 0x29D295, 0x29CBC1,
	0x29C4EF, 0x29BE1F, 0x29B752, 0x29B087, 0x29A9BE, 0x29A2F7, 0x299C33, 0x299571,
	0x298EB0, 0x2987F3, 0x298137, 0x297A7D, 0x2973C6, 0x296D11, 0x29665E, 0x295FAD,
	0x2958FE, 0x295251, 0x294BA7, 0x2944FF, 0x293E59, 0x2937B5, 0x293113, 0x292A73,
	0x2923D6, 0x291D3A, 0x2916A1, 0x29100A, 0x290975, 0x2902E2, 0x28FC51, 0x28F5C2,
	0x28EF35, 0x28E8AB, 0x28E222, 0x28DB9C, 0x28D518, 0x28CE95, 0x28C815, 0x28C197,
	0x28BB1B, 0x28B4A1, 0x28AE29, 0x28A7B3, 0x28A13F, 0x289ACE, 0x28945E, 0x288DF0,
	0x288785, 0x28811B, 0x287AB3, 0x28744E, 0x286DEA, 0x286789, 0x286129, 0x285ACC,
	0x285470, 0x284E17, 0x2847BF, 0x28416A, 0x283B16, 0x2834C5, 0x282E75, 0x282828,
	0x2821DC, 0x281B92, 0x28154B, 0x280F05, 0x2808C1, 0x280280, 0x27FC40, 0x27F602,
	0x27EFC6, 0x27E98C, 0x27E354, 0x27DD1E, 0x27D6EA, 0x27D0B8, 0x27CA87, 0x27C459,
	0x27BE2D, 0x27B802, 0x27B1D9, 0x27ABB3, 0x27A58E, 0x279F6B, 0x27994A, 0x27932B,
	0x278D0E, 0x2786F2, 0x2780D9, 0x277AC1, 0x2774AC, 0x276E98, 0x276886, 0x276276,
	0x275C67, 0x27565B, 0x275051, 0x274A48, 0x274441, 0x273E3C, 0x273839, 0x273238,
	0x272C38, 0x27263B, 0x27203F, 0x271A45, 0x27144D, 0x270E57, 0x270862, 0x270270,
	0x26FC7F, 0x26F690, 0x26F0A3, 0x26EAB7, 0x26E4CE, 0x26DEE6, 0x26D900, 0x26D31B,
	0x26CD39, 0x26C758, 0x26C179, 0x26BB9C, 0x26B5C1, 0x26AFE7, 0x26AA10, 0x26A439,
	0x269E65, 0x269893, 0x2692C2, 0x268CF3, 0x268726, 0x26815A, 0x267B90, 0x2675C8,
	0x267002, 0x266A3D, 0x26647A, 0x265EB9, 0x2658FA, 0x26533C, 0x264D80, 0x2647C6,
	0x26420E, 0x263C57, 0x2636A2, 0x2630EE, 0x262B3C, 0x26258C, 0x261FDE, 0x261A32,
	0x261487, 0x260EDD, 0x260936, 0x260390, 0x25FDEC, 0x25F849, 0x25F2A8, 0x25ED09,
	0x25E76B, 0x25E1D0, 0x25DC35, 0x25D69D, 0x25D106, 0x25CB71, 0x25C5DD, 0x25C04B,
	0x25BABB, 0x25B52C, 0x25AF9F, 0x25AA14, 0x25A48A, 0x259F02, 0x25997B, 0x2593F6,
	0x258E73, 0x2588F1, 0x258371, 0x257DF3, 0x257876, 0x2572FB, 0x256D81, 0x256809,
	0x256292, 0x255D1E, 0x2557AA, 0x255239, 0x254CC9, 0x25475A, 0x2541ED, 0x253C82,
	0x253718, 0x2531B0, 0x252C49, 0x2526E4, 0x252181, 0x251C1F, 0x2516BE, 0x251160,
	0x250C02, 0x2506A7, 0x25014D, 0x24FBF4, 0x24F69D, 0x24F147, 0x24EBF3, 0x24E6A1,
	0x24E150, 0x24DC01, 0x24D6B3, 0x24D166, 0x24CC1C, 0x24C6D2, 0x24C18B, 0x24BC44,
	0x24B700, 0x24B1BC, 0x24AC7B, 0x24A73A, 0x24A1FC, 0x249CBF, 0x249783, 0x249249,
	0x248D10, 0x2487D9, 0x2482A3, 0x247D6F, 0x24783C, 0x24730B, 0x246DDB, 0x2468AC,
	0x246380, 0x245E54, 0x24592A, 0x245402, 0x244EDB, 0x2449B5, 0x244491, 0x243F6F,
	0x243A4D, 0x24352E, 0x24300F, 0x242AF3, 0x2425D7, 0x2420BD, 0x241BA5, 0x24168E,
	0x241178, 0x240C64, 0x240751, 0x240240, 0x23FD30, 0x23F821, 0x23F314, 0x23EE08,
	0x23E8FE, 0x23E3F5, 0x23DEEE, 0x23D9E8, 0x23D4E3, 0x23CFE0, 0x23CADE, 0x23C5DE,
	0x23C0DF, 0x23BBE1, 0x23B6E5, 0x23B1EA, 0x23ACF1, 0x23A7F9, 0x23A302, 0x239E0D,
	0x239919, 0x239426, 0x238F35, 0x238A45, 0x238557, 0x23806A, 0x237B7E, 0x237694,
	0x2371AB, 0x236CC3, 0x2367DD, 0x2362F8, 0x235E15, 0x235933, 0x235452, 0x234F72,
	0x234A94, 0x2345B7, 0x2340DC, 0x233C02, 0x233729, 0x233251, 0x232D7B, 0x2328A7,
	0x2323D3, 0x231F01, 0x231A30, 0x231561, 0x231092, 0x230BC5, 0x2306FA, 0x230230,
	0x22FD67, 0x22F89F, 0x22F3D9, 0x22EF14, 0x22EA50, 0x22E58E, 0x22E0CC, 0x22DC0D,
	0x22D74E, 0x22D291, 0x22CDD5, 0x22C91A, 0x22C461, 0x22BFA9, 0x22BAF2, 0x22B63C,
	0x22B188, 0x22ACD5, 0x22A823, 0x22A373, 0x229EC4, 0x229A16, 0x229569, 0x2290BE,
	0x228C13, 0x22876B, 0x2282C3, 0x227E1D, 0x227977, 0x2274D4, 0x227031, 0x226B90,
	0x2266F0, 0x226251, 0x225DB3, 0x225917, 0x22547B, 0x224FE1, 0x224B49, 0x2246B1,
	0x22421B, 0x223D86, 0x2238F2, 0x223460, 0x222FCE, 0x222B3E, 0x2226AF, 0x222222,
	0x221D95, 0x22190A, 0x221480, 0x220FF7, 0x220B6F, 0x2206E9, 0x220264, 0x21FDE0,
	0x21F95D, 0x21F4DB, 0x21F05B, 0x21EBDB, 0x21E75D, 0x21E2E1, 0x21DE65, 0x21D9EA,
	0x21D571, 0x21D0F9, 0x21CC82, 0x21C80C, 0x21C398, 0x21BF24, 0x21BAB2, 0x21B641,
	0x21B1D1, 0x21AD62, 0x21A8F5, 0x21A488, 0x21A01D, 0x219BB3, 0x21974A, 0x2192E2,
	0x218E7C, 0x218A16, 0x2185B2, 0x21814F, 0x217CED, 0x21788C, 0x21742C, 0x216FCD,
	0x216B70, 0x216714, 0x2162B8, 0x215E5E, 0x215A05, 0x2155AE, 0x215157, 0x214D02,
	0x2148AD, 0x21445A, 0x214008, 0x213BB7, 0x213767, 0x213318, 0x212ECA, 0x212A7E,
	0x212633, 0x2121E8, 0x211D9F, 0x211957, 0x211510, 0x2110CA, 0x210C85, 0x210842,
	0x2103FF, 0x20FFBE, 0x20FB7D, 0x20F73E, 0x20F300, 0x20EEC3, 0x20EA87, 0x20E64C,
	0x20E212, 0x20DDD9, 0x20D9A1, 0x20D56B, 0x20D135, 0x20CD01, 0x20C8CD, 0x20C49B,
	0x20C06A, 0x20BC3A, 0x20B80B, 0x20B3DD, 0x20AFB0, 0x20AB84, 0x20A759, 0x20A32F,
	0x209F07, 0x209ADF, 0x2096B9, 0x209293, 0x208E6F, 0x208A4B, 0x208629, 0x208208,
	0x207DE7, 0x2079C8, 0x2075AA, 0x20718D, 0x206D71, 0x206956, 0x20653C, 0x206123,
	0x205D0B, 0x2058F4, 0x2054DE, 0x2050C9, 0x204CB6, 0x2048A3, 0x204491, 0x204081,
	0x203C71, 0x203862, 0x203455, 0x203048, 0x202C3C, 0x202832, 0x202428, 0x202020,
	0x201C18, 0x201812, 0x20140C, 0x201008, 0x200C04, 0x200802, 0x200400
};