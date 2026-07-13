{
	"patcher" : 	{
		"rect" : [ 350.0, 44.0, 663.0, 452.0 ],
		"bgcolor" : [ 1.0, 1.0, 1.0, 1.0 ],
		"bglocked" : 0,
		"defrect" : [ 350.0, 44.0, 663.0, 452.0 ],
		"openinpresentation" : 0,
		"default_fontsize" : 10.0,
		"default_fontface" : 0,
		"default_fontname" : "Arial",
		"gridonopen" : 0,
		"gridsize" : [ 15.0, 15.0 ],
		"gridsnaponopen" : 0,
		"toolbarvisible" : 1,
		"boxfadetime" : 200,
		"boxanimatetime" : 200,
		"scrollanimatetime" : 200,
		"metadata" : [  ],
		"boxes" : [ 			{
				"box" : 				{
					"maxclass" : "comment",
					"text" : "-2uptech",
					"numinlets" : 1,
					"id" : "obj-31",
					"numoutlets" : 0,
					"fontsize" : 10.0,
					"patching_rect" : [ 581.0, 417.0, 56.0, 20.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "comment",
					"text" : "amplitude response is logarithmic too.",
					"numinlets" : 1,
					"id" : "obj-30",
					"numoutlets" : 0,
					"fontsize" : 10.0,
					"patching_rect" : [ 243.0, 353.0, 203.0, 20.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "comment",
					"text" : "read buffer into jit matrix, frames triggered by ramp",
					"linecount" : 2,
					"numinlets" : 1,
					"id" : "obj-29",
					"numoutlets" : 0,
					"fontsize" : 10.0,
					"patching_rect" : [ 449.0, 275.0, 139.0, 33.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "comment",
					"text" : "looks up from first half of spectrum",
					"linecount" : 2,
					"numinlets" : 1,
					"id" : "obj-28",
					"numoutlets" : 0,
					"fontsize" : 10.0,
					"patching_rect" : [ 491.0, 231.0, 100.0, 33.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "comment",
					"text" : "output [-1. - 0.]",
					"numinlets" : 1,
					"id" : "obj-27",
					"numoutlets" : 0,
					"fontsize" : 10.0,
					"patching_rect" : [ 491.0, 202.0, 84.0, 20.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "comment",
					"text" : "lin\/log frequency (default log)",
					"numinlets" : 1,
					"id" : "obj-26",
					"numoutlets" : 0,
					"fontsize" : 10.0,
					"patching_rect" : [ 491.0, 149.0, 158.0, 20.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "outlet",
					"numinlets" : 1,
					"id" : "obj-25",
					"numoutlets" : 0,
					"patching_rect" : [ 45.0, 404.0, 25.0, 25.0 ],
					"comment" : "spectral data out"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "outlet",
					"numinlets" : 1,
					"id" : "obj-24",
					"numoutlets" : 0,
					"patching_rect" : [ 227.0, 404.0, 25.0, 25.0 ],
					"comment" : "ramp out"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "newobj",
					"text" : "jit.op @op * @val 0.25",
					"numinlets" : 2,
					"id" : "obj-23",
					"numoutlets" : 2,
					"fontsize" : 10.0,
					"outlettype" : [ "jit_matrix", "" ],
					"patching_rect" : [ 45.0, 379.0, 126.0, 20.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "newobj",
					"text" : "jit.op @op log",
					"numinlets" : 2,
					"id" : "obj-22",
					"numoutlets" : 2,
					"fontsize" : 10.0,
					"outlettype" : [ "jit_matrix", "" ],
					"patching_rect" : [ 45.0, 354.0, 82.0, 20.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "newobj",
					"text" : "jit.op @op + @val 1.",
					"numinlets" : 2,
					"id" : "obj-21",
					"numoutlets" : 2,
					"fontsize" : 10.0,
					"outlettype" : [ "jit_matrix", "" ],
					"patching_rect" : [ 45.0, 329.0, 116.0, 20.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "newobj",
					"text" : "jit.unpack 2",
					"numinlets" : 1,
					"id" : "obj-20",
					"numoutlets" : 3,
					"fontsize" : 10.0,
					"outlettype" : [ "jit_matrix", "jit_matrix", "" ],
					"patching_rect" : [ 20.0, 299.0, 69.0, 20.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "comment",
					"text" : "pfft~ subpatch performs windowing and overlap, puts magnitudes of real spectrum into unique buffer.",
					"linecount" : 4,
					"numinlets" : 1,
					"id" : "obj-19",
					"numoutlets" : 0,
					"fontsize" : 10.0,
					"patching_rect" : [ 29.0, 173.0, 156.0, 60.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "newobj",
					"text" : "lookup~ specamp 0 512",
					"numinlets" : 3,
					"id" : "obj-18",
					"numoutlets" : 1,
					"fontsize" : 10.0,
					"outlettype" : [ "signal" ],
					"patching_rect" : [ 356.0, 231.0, 137.0, 20.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "newobj",
					"text" : "-~ 1.",
					"numinlets" : 2,
					"id" : "obj-17",
					"numoutlets" : 1,
					"fontsize" : 10.0,
					"outlettype" : [ "signal" ],
					"patching_rect" : [ 356.0, 202.0, 137.0, 20.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "inlet",
					"numinlets" : 0,
					"id" : "obj-16",
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 398.0, 39.0, 25.0, 25.0 ],
					"comment" : "lin\/log frequency display"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "newobj",
					"text" : "loadmess 1",
					"numinlets" : 1,
					"id" : "obj-15",
					"numoutlets" : 1,
					"fontsize" : 10.0,
					"outlettype" : [ "" ],
					"patching_rect" : [ 423.0, 149.0, 70.0, 20.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "toggle",
					"numinlets" : 1,
					"id" : "obj-14",
					"numoutlets" : 1,
					"outlettype" : [ "int" ],
					"patching_rect" : [ 398.0, 149.0, 20.0, 20.0 ]
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "newobj",
					"text" : "p logtable",
					"numinlets" : 2,
					"id" : "obj-13",
					"numoutlets" : 1,
					"fontsize" : 10.0,
					"outlettype" : [ "signal" ],
					"patching_rect" : [ 356.0, 173.0, 61.0, 20.0 ],
					"fontname" : "Arial",
					"patcher" : 					{
						"rect" : [ 492.0, 45.0, 478.0, 324.0 ],
						"bgcolor" : [ 1.0, 1.0, 1.0, 1.0 ],
						"bglocked" : 0,
						"defrect" : [ 492.0, 45.0, 478.0, 324.0 ],
						"openinpresentation" : 0,
						"default_fontsize" : 10.0,
						"default_fontface" : 0,
						"default_fontname" : "Arial",
						"gridonopen" : 0,
						"gridsize" : [ 15.0, 15.0 ],
						"gridsnaponopen" : 0,
						"toolbarvisible" : 1,
						"boxfadetime" : 200,
						"boxanimatetime" : 200,
						"scrollanimatetime" : 200,
						"metadata" : [  ],
						"boxes" : [ 							{
								"box" : 								{
									"maxclass" : "outlet",
									"numinlets" : 1,
									"id" : "obj-19",
									"numoutlets" : 0,
									"patching_rect" : [ 15.0, 287.0, 25.0, 25.0 ],
									"comment" : ""
								}

							}
, 							{
								"box" : 								{
									"maxclass" : "newobj",
									"text" : "lookup~ logbuf 0 256",
									"numinlets" : 3,
									"id" : "obj-17",
									"numoutlets" : 1,
									"fontsize" : 10.0,
									"outlettype" : [ "signal" ],
									"patching_rect" : [ 46.0, 193.0, 117.0, 20.0 ],
									"fontname" : "Arial"
								}

							}
, 							{
								"box" : 								{
									"maxclass" : "newobj",
									"text" : "-~ 1.",
									"numinlets" : 2,
									"id" : "obj-16",
									"numoutlets" : 1,
									"fontsize" : 10.0,
									"outlettype" : [ "signal" ],
									"patching_rect" : [ 46.0, 164.0, 34.0, 20.0 ],
									"fontname" : "Arial"
								}

							}
, 							{
								"box" : 								{
									"maxclass" : "newobj",
									"text" : "*~ 2.",
									"numinlets" : 2,
									"id" : "obj-15",
									"numoutlets" : 1,
									"fontsize" : 10.0,
									"outlettype" : [ "signal" ],
									"patching_rect" : [ 46.0, 135.0, 35.0, 20.0 ],
									"fontname" : "Arial"
								}

							}
, 							{
								"box" : 								{
									"maxclass" : "newobj",
									"text" : "buffer~ logbuf 1000",
									"numinlets" : 1,
									"id" : "obj-14",
									"numoutlets" : 2,
									"fontsize" : 10.0,
									"outlettype" : [ "float", "bang" ],
									"patching_rect" : [ 157.0, 265.0, 111.0, 20.0 ],
									"fontname" : "Arial"
								}

							}
, 							{
								"box" : 								{
									"maxclass" : "newobj",
									"text" : "peek~ logbuf",
									"numinlets" : 3,
									"id" : "obj-13",
									"numoutlets" : 1,
									"fontsize" : 10.0,
									"outlettype" : [ "float" ],
									"patching_rect" : [ 157.0, 239.0, 77.0, 20.0 ],
									"fontname" : "Arial"
								}

							}
, 							{
								"box" : 								{
									"maxclass" : "comment",
									"text" : "inverse log function over audio spectrum scaled to about [0, 1]",
									"linecount" : 2,
									"numinlets" : 1,
									"id" : "obj-12",
									"numoutlets" : 0,
									"fontsize" : 10.0,
									"patching_rect" : [ 269.0, 167.0, 169.0, 33.0 ],
									"fontname" : "Arial"
								}

							}
, 							{
								"box" : 								{
									"maxclass" : "newobj",
									"text" : "\/ 10000.",
									"numinlets" : 2,
									"id" : "obj-11",
									"numoutlets" : 1,
									"fontsize" : 10.0,
									"outlettype" : [ "float" ],
									"patching_rect" : [ 249.0, 143.0, 52.0, 20.0 ],
									"fontname" : "Arial"
								}

							}
, 							{
								"box" : 								{
									"maxclass" : "newobj",
									"text" : "expr pow(10.\\, ($f1 \/ 106.34 + 1.60205))",
									"numinlets" : 1,
									"id" : "obj-10",
									"numoutlets" : 1,
									"fontsize" : 10.0,
									"outlettype" : [ "" ],
									"patching_rect" : [ 249.0, 116.0, 215.0, 20.0 ],
									"fontname" : "Arial"
								}

							}
, 							{
								"box" : 								{
									"maxclass" : "newobj",
									"text" : "- 1.",
									"numinlets" : 2,
									"id" : "obj-9",
									"numoutlets" : 1,
									"fontsize" : 10.0,
									"outlettype" : [ "float" ],
									"patching_rect" : [ 249.0, 90.0, 32.5, 20.0 ],
									"fontname" : "Arial"
								}

							}
, 							{
								"box" : 								{
									"maxclass" : "newobj",
									"text" : "t i i",
									"numinlets" : 1,
									"id" : "obj-8",
									"numoutlets" : 2,
									"fontsize" : 10.0,
									"outlettype" : [ "int", "int" ],
									"patching_rect" : [ 236.0, 65.0, 32.5, 20.0 ],
									"fontname" : "Arial"
								}

							}
, 							{
								"box" : 								{
									"maxclass" : "button",
									"color" : [ 0.082353, 0.25098, 0.431373, 1.0 ],
									"numinlets" : 1,
									"id" : "obj-7",
									"numoutlets" : 1,
									"outlettype" : [ "bang" ],
									"patching_rect" : [ 184.0, 11.0, 20.0, 20.0 ]
								}

							}
, 							{
								"box" : 								{
									"maxclass" : "newobj",
									"text" : "uzi 256",
									"numinlets" : 2,
									"id" : "obj-6",
									"numoutlets" : 3,
									"fontsize" : 10.0,
									"outlettype" : [ "bang", "bang", "int" ],
									"patching_rect" : [ 207.0, 37.0, 48.0, 20.0 ],
									"fontname" : "Arial"
								}

							}
, 							{
								"box" : 								{
									"maxclass" : "newobj",
									"text" : "loadbang",
									"numinlets" : 1,
									"id" : "obj-5",
									"numoutlets" : 1,
									"fontsize" : 10.0,
									"outlettype" : [ "bang" ],
									"patching_rect" : [ 207.0, 11.0, 58.0, 20.0 ],
									"fontname" : "Arial"
								}

							}
, 							{
								"box" : 								{
									"maxclass" : "newobj",
									"text" : "+ 1",
									"numinlets" : 2,
									"id" : "obj-4",
									"numoutlets" : 1,
									"fontsize" : 10.0,
									"outlettype" : [ "int" ],
									"patching_rect" : [ 112.0, 51.0, 32.5, 20.0 ],
									"fontname" : "Arial"
								}

							}
, 							{
								"box" : 								{
									"maxclass" : "inlet",
									"numinlets" : 0,
									"id" : "obj-3",
									"numoutlets" : 1,
									"outlettype" : [ "int" ],
									"patching_rect" : [ 112.0, 11.0, 25.0, 25.0 ],
									"comment" : ""
								}

							}
, 							{
								"box" : 								{
									"maxclass" : "newobj",
									"text" : "gate~ 2",
									"numinlets" : 2,
									"id" : "obj-2",
									"numoutlets" : 2,
									"fontsize" : 10.0,
									"outlettype" : [ "signal", "signal" ],
									"patching_rect" : [ 15.0, 84.0, 50.0, 20.0 ],
									"fontname" : "Arial"
								}

							}
, 							{
								"box" : 								{
									"maxclass" : "inlet",
									"numinlets" : 0,
									"id" : "obj-1",
									"numoutlets" : 1,
									"outlettype" : [ "signal" ],
									"patching_rect" : [ 46.0, 11.0, 25.0, 25.0 ],
									"comment" : ""
								}

							}
 ],
						"lines" : [ 							{
								"patchline" : 								{
									"source" : [ "obj-17", 0 ],
									"destination" : [ "obj-19", 0 ],
									"hidden" : 0
								}

							}
, 							{
								"patchline" : 								{
									"source" : [ "obj-2", 0 ],
									"destination" : [ "obj-19", 0 ],
									"hidden" : 0
								}

							}
, 							{
								"patchline" : 								{
									"source" : [ "obj-2", 1 ],
									"destination" : [ "obj-15", 0 ],
									"hidden" : 0
								}

							}
, 							{
								"patchline" : 								{
									"source" : [ "obj-15", 0 ],
									"destination" : [ "obj-16", 0 ],
									"hidden" : 0
								}

							}
, 							{
								"patchline" : 								{
									"source" : [ "obj-16", 0 ],
									"destination" : [ "obj-17", 0 ],
									"hidden" : 0
								}

							}
, 							{
								"patchline" : 								{
									"source" : [ "obj-8", 0 ],
									"destination" : [ "obj-13", 0 ],
									"hidden" : 0,
									"midpoints" : [ 245.5, 88.0, 166.5, 88.0 ]
								}

							}
, 							{
								"patchline" : 								{
									"source" : [ "obj-11", 0 ],
									"destination" : [ "obj-13", 1 ],
									"hidden" : 0,
									"midpoints" : [ 258.5, 212.0, 195.5, 212.0 ]
								}

							}
, 							{
								"patchline" : 								{
									"source" : [ "obj-10", 0 ],
									"destination" : [ "obj-11", 0 ],
									"hidden" : 0
								}

							}
, 							{
								"patchline" : 								{
									"source" : [ "obj-6", 2 ],
									"destination" : [ "obj-8", 0 ],
									"hidden" : 0
								}

							}
, 							{
								"patchline" : 								{
									"source" : [ "obj-8", 1 ],
									"destination" : [ "obj-9", 0 ],
									"hidden" : 0
								}

							}
, 							{
								"patchline" : 								{
									"source" : [ "obj-9", 0 ],
									"destination" : [ "obj-10", 0 ],
									"hidden" : 0
								}

							}
, 							{
								"patchline" : 								{
									"source" : [ "obj-7", 0 ],
									"destination" : [ "obj-6", 0 ],
									"hidden" : 0
								}

							}
, 							{
								"patchline" : 								{
									"source" : [ "obj-5", 0 ],
									"destination" : [ "obj-6", 0 ],
									"hidden" : 0
								}

							}
, 							{
								"patchline" : 								{
									"source" : [ "obj-4", 0 ],
									"destination" : [ "obj-2", 0 ],
									"hidden" : 0,
									"midpoints" : [ 121.5, 77.0, 24.5, 77.0 ]
								}

							}
, 							{
								"patchline" : 								{
									"source" : [ "obj-3", 0 ],
									"destination" : [ "obj-4", 0 ],
									"hidden" : 0
								}

							}
, 							{
								"patchline" : 								{
									"source" : [ "obj-1", 0 ],
									"destination" : [ "obj-2", 1 ],
									"hidden" : 0
								}

							}
 ]
					}
,
					"saved_object_attributes" : 					{
						"globalpatchername" : "",
						"fontface" : 0,
						"default_fontname" : "Arial",
						"fontsize" : 10.0,
						"default_fontface" : 0,
						"default_fontsize" : 10.0,
						"fontname" : "Arial"
					}

				}

			}
, 			{
				"box" : 				{
					"maxclass" : "newobj",
					"text" : "\/~ 256.",
					"numinlets" : 2,
					"id" : "obj-12",
					"numoutlets" : 1,
					"fontsize" : 10.0,
					"outlettype" : [ "signal" ],
					"patching_rect" : [ 356.0, 120.0, 46.0, 20.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "newobj",
					"text" : "count~ 0 256 1 1",
					"numinlets" : 2,
					"id" : "obj-11",
					"numoutlets" : 1,
					"fontsize" : 10.0,
					"outlettype" : [ "signal" ],
					"patching_rect" : [ 356.0, 91.0, 97.0, 20.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "newobj",
					"text" : "jit.catch~ 2 @mode 3 @framesize 256 @trigchan 0 @trigdir 0 @trigthresh 0.4999",
					"numinlets" : 2,
					"id" : "obj-10",
					"numoutlets" : 2,
					"fontsize" : 10.0,
					"outlettype" : [ "jit_matrix", "" ],
					"patching_rect" : [ 20.0, 274.0, 429.0, 20.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "newobj",
					"text" : "prepend set",
					"numinlets" : 1,
					"id" : "obj-9",
					"numoutlets" : 1,
					"fontsize" : 10.0,
					"outlettype" : [ "" ],
					"patching_rect" : [ 182.0, 157.0, 72.0, 20.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "newobj",
					"text" : "pfft~ amp_pfft 512 2",
					"numinlets" : 2,
					"id" : "obj-8",
					"numoutlets" : 0,
					"fontsize" : 10.0,
					"patching_rect" : [ 66.0, 157.0, 114.0, 20.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "newobj",
					"text" : "t l l",
					"numinlets" : 1,
					"id" : "obj-7",
					"numoutlets" : 2,
					"fontsize" : 10.0,
					"outlettype" : [ "", "" ],
					"patching_rect" : [ 148.0, 111.0, 31.5, 20.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "comment",
					"text" : "unique spectrum buffer name",
					"numinlets" : 1,
					"id" : "obj-6",
					"numoutlets" : 0,
					"fontsize" : 10.0,
					"patching_rect" : [ 157.0, 67.0, 160.0, 20.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "message",
					"text" : "#0-buf",
					"numinlets" : 2,
					"id" : "obj-5",
					"numoutlets" : 1,
					"fontsize" : 10.0,
					"outlettype" : [ "" ],
					"patching_rect" : [ 148.0, 87.0, 43.0, 18.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "newobj",
					"text" : "loadbang",
					"numinlets" : 1,
					"id" : "obj-4",
					"numoutlets" : 1,
					"fontsize" : 10.0,
					"outlettype" : [ "bang" ],
					"patching_rect" : [ 148.0, 48.0, 58.0, 20.0 ],
					"fontname" : "Arial"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "inlet",
					"numinlets" : 0,
					"id" : "obj-3",
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 66.0, 39.0, 25.0, 25.0 ],
					"comment" : "audio in"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "inlet",
					"numinlets" : 0,
					"id" : "obj-2",
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 20.0, 39.0, 25.0, 25.0 ],
					"comment" : "bang to send spectrum frame out"
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "comment",
					"text" : "see \"audioviz_simple_jit_fft\" patch for an example",
					"numinlets" : 1,
					"id" : "obj-1",
					"numoutlets" : 0,
					"fontsize" : 10.0,
					"patching_rect" : [ 17.0, 16.0, 267.0, 20.0 ],
					"fontname" : "Arial"
				}

			}
 ],
		"lines" : [ 			{
				"patchline" : 				{
					"source" : [ "obj-20", 0 ],
					"destination" : [ "obj-24", 0 ],
					"hidden" : 0,
					"midpoints" : [ 29.5, 324.0, 236.5, 324.0 ]
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-23", 0 ],
					"destination" : [ "obj-25", 0 ],
					"hidden" : 0
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-22", 0 ],
					"destination" : [ "obj-23", 0 ],
					"hidden" : 0
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-21", 0 ],
					"destination" : [ "obj-22", 0 ],
					"hidden" : 0
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-20", 1 ],
					"destination" : [ "obj-21", 0 ],
					"hidden" : 0
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-10", 0 ],
					"destination" : [ "obj-20", 0 ],
					"hidden" : 0
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-12", 0 ],
					"destination" : [ "obj-10", 0 ],
					"hidden" : 0
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-9", 0 ],
					"destination" : [ "obj-18", 0 ],
					"hidden" : 0
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-18", 0 ],
					"destination" : [ "obj-10", 1 ],
					"hidden" : 0
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-17", 0 ],
					"destination" : [ "obj-18", 0 ],
					"hidden" : 0
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-13", 0 ],
					"destination" : [ "obj-17", 0 ],
					"hidden" : 0
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-16", 0 ],
					"destination" : [ "obj-14", 0 ],
					"hidden" : 0
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-15", 0 ],
					"destination" : [ "obj-14", 0 ],
					"hidden" : 0
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-14", 0 ],
					"destination" : [ "obj-13", 1 ],
					"hidden" : 0
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-12", 0 ],
					"destination" : [ "obj-13", 0 ],
					"hidden" : 0
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-11", 0 ],
					"destination" : [ "obj-12", 0 ],
					"hidden" : 0
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-2", 0 ],
					"destination" : [ "obj-10", 0 ],
					"hidden" : 0
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-7", 0 ],
					"destination" : [ "obj-9", 0 ],
					"hidden" : 0,
					"midpoints" : [ 157.5, 145.0, 191.5, 145.0 ]
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-7", 1 ],
					"destination" : [ "obj-8", 1 ],
					"hidden" : 0
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-5", 0 ],
					"destination" : [ "obj-7", 0 ],
					"hidden" : 0
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-3", 0 ],
					"destination" : [ "obj-8", 0 ],
					"hidden" : 0
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-4", 0 ],
					"destination" : [ "obj-5", 0 ],
					"hidden" : 0
				}

			}
 ]
	}

}
