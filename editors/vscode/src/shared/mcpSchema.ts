import { z, type ZodRawShape, type ZodTypeAny } from 'zod';
import { type SemdlJsonObjectSchema, type SemdlJsonSchema } from './toolContracts';

export function jsonObjectSchemaToZodObject(schema: SemdlJsonObjectSchema) {
  const objectSchema = z.object(jsonObjectSchemaToZodShape(schema));
  return schema.additionalProperties === false ? objectSchema.strict() : objectSchema;
}

export function validateMcpToolInput(schema: SemdlJsonObjectSchema, input: Record<string, unknown>): Record<string, unknown> {
  return jsonObjectSchemaToZodObject(schema).parse(input) as Record<string, unknown>;
}

export function jsonObjectSchemaToZodShape(schema: SemdlJsonObjectSchema): ZodRawShape {
  const required = new Set(schema.required ?? []);
  return Object.fromEntries(
    Object.entries(schema.properties).map(([name, propertySchema]) => {
      let propertyType = jsonSchemaToZodType(propertySchema);
      if (!required.has(name)) {
        propertyType = propertyType.optional();
      }
      return [name, propertyType];
    })
  );
}

export function jsonSchemaToZodType(schema: SemdlJsonSchema): ZodTypeAny {
  if (schema.type === 'object') {
    return jsonObjectSchemaToZodObject(schema);
  }

  let zodType: ZodTypeAny;
  if (schema.enum && schema.enum.length > 0) {
    zodType = z.enum([schema.enum[0], ...schema.enum.slice(1)]);
  } else {
    zodType = z.string();
  }

  if (schema.description) {
    zodType = zodType.describe(schema.description);
  }
  return zodType;
}