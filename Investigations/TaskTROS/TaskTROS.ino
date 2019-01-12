


// -- Task handles for use in the notifications
static TaskHandle_t core0TaskAHandle = 0;
static TaskHandle_t core0TaskBHandle = 0;
static TaskHandle_t userTaskHandleForTaskA = 0;
static TaskHandle_t userTaskHandleForTaskB = 0;

void InvokeTask_A_OnCore0() {
	if ( userTaskHandleForTaskA == 0 ) {
       // -- Store the handle of the current task, so that the show task can notify it when it's done
        userTaskHandleForTaskA = xTaskGetCurrentTaskHandle();
		// make sure is not null otherwise will fail
		if ( core0TaskAHandle != NULL ) {
        // -- Trigger the show task
			Serial.printf("Invoking task A on Core0\n");
			xTaskNotifyGive(core0TaskAHandle);
			// -- Wait to be notified that it's done
			const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 200 );
			ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
			userTaskHandleForTaskA = 0;	
		}
	}
}

void InvokeTask_B_OnCore0() {
	if ( userTaskHandleForTaskB == 0 ) {
       // -- Store the handle of the current task, so that the show task can notify it when it's done
        userTaskHandleForTaskB = xTaskGetCurrentTaskHandle();
		// make sure is not null otherwise will fail
		if ( core0TaskBHandle != NULL ) {
        // -- Trigger the show task
			Serial.printf("Invoking task B on Core0\n");
			xTaskNotify( core0TaskBHandle, 0, eNoAction );
			// -- Wait to be notified that it's done
			// const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 200 );
			// ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
			userTaskHandleForTaskB = 0;	
		}
	}
}

//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
void setup() {

	Serial.begin(9600);


	xTaskCreatePinnedToCore (
		core0TaskA,	// function
		"Task_A",		// name
		10000,			// stack
		NULL,			// parameter
		2,				// priority
		&core0TaskAHandle,	// handle
		0				// core#
	);	

	xTaskCreatePinnedToCore (
		core0TaskB,	// function
		"Task_B",		// name
		10000,			// stack
		NULL,			// parameter
		2,				// priority
		&core0TaskBHandle,	// handle
		0				// core#
	);	

}
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------

long taskAms = 0;
long taskBms = 0;

void loop() {

	if ( millis()-taskAms > 2000 ) {
		taskAms = millis();
		InvokeTask_A_OnCore0();
	}

	if ( millis()-taskBms > 2300) {
		taskBms = millis();
		InvokeTask_B_OnCore0();
	}

	vTaskDelay( 10 );

}
/**************************************************************
					TASK 0
**************************************************************/

long core0TaskAMs = 0;
long core0TaskBmillis = 0;

void core0TaskA( void *parameter ) {

	for (;;) {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		Serial.printf("Handling task A\n");

		// // -- Notify the calling task
		xTaskNotifyGive(userTaskHandleForTaskA);
		//vTaskDelay( 10 );

		if ( millis()-core0TaskAMs > 2000 ) {
			core0TaskAMs = millis();
			Serial.printf("Loop line: Task A\n");
		}
	}

	vTaskDelete(NULL);
}

void core0TaskB( void *parameter ) {


	BaseType_t xResult;
	const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 10000 );
	uint32_t ulNotifiedValue;

	for (;;) {

		xResult = xTaskNotifyWait( pdFALSE,    /* Don't clear bits on entry. */
		                   ULONG_MAX,        /* Clear all bits on exit. */
		                   &ulNotifiedValue, /* Stores the notified value. */
		                   xMaxBlockTime );

		if( xResult == pdPASS )
		{
			Serial.printf("Handling task B\n");
		}
		else {
			Serial.printf("xResult != pdPASS\n");	
		 /* A notification was received.  See which bits were set. */
		}

		// ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		// Serial.printf("Handling task B\n");

		// // // -- Notify the calling task
		// xTaskNotifyGive(userTaskHandleForTaskB);
		// //vTaskDelay( 10 );

		// if ( millis()-core0TaskBmillis > 2000 ) {
		// 	core0TaskBmillis = millis();
		// 	Serial.printf("Loop line: Task B\n");
		// }
	}

	vTaskDelete(NULL);
}
